const std = @import("std");

const Entry = struct {
    word: []const u8,
    category: []const u8,
    clue1: []const u8,
    clue2: []const u8,
    clue3: []const u8,
    surprisal: f64,
};

pub fn build(b: *std.Build) void {
    const io = b.graph.io;
    const root_dir = b.build_root.handle;

    ///////////////////////////////////////////////////////////////////////////
    // Create the clue dataset
    const header_path = "src" ++ std.fs.path.sep_str ++ "clues.h";
    root_dir.access(io, header_path, .{}) catch {
        const allocator = b.allocator;

        const word_path = "data" ++ std.fs.path.sep_str ++ "clues.csv";
        const word_file = root_dir.readFileAlloc(io, word_path, allocator, .limited(10 * 1024 * 1024)) catch @panic("failed to read clues.csv");

        var entries: std.ArrayList(Entry) = .empty;
        defer entries.deinit(allocator);

        var line_iter = std.mem.splitScalar(u8, word_file, '\n');
        while (line_iter.next()) |line| {
            if (line.len == 0) continue;

            var fields: [6][]const u8 = .{ "", "", "", "", "", "" };
            var field_idx: usize = 0;
            var i: usize = 0;
            var field_start: usize = 0;
            var in_quotes = false;

            while (i < line.len and field_idx < 6) : (i += 1) {
                const c = line[i];
                if (c == '"') {
                    in_quotes = !in_quotes;
                } else if (c == ',' and !in_quotes) {
                    var field = line[field_start..i];
                    if (field.len >= 2 and field[0] == '"' and field[field.len - 1] == '"') {
                        field = field[1 .. field.len - 1];
                    }
                    fields[field_idx] = field;
                    field_idx += 1;
                    field_start = i + 1;
                }
            }

            if (field_idx < 6) {
                var field = line[field_start..];
                if (field.len >= 2 and field[0] == '"' and field[field.len - 1] == '"') {
                    field = field[1 .. field.len - 1];
                }
                fields[field_idx] = field;
            }

            const surprisal = -@log(std.fmt.parseFloat(f64, fields[1]) catch @panic("unable to calculate surprisal"));
            const word_upper = allocator.alloc(u8, fields[0].len) catch @panic("alloc failed");
            for (fields[0], 0..) |c, j| {
                word_upper[j] = std.ascii.toUpper(c);
            }

            entries.append(allocator, .{
                .word = word_upper,
                .category = fields[2],
                .clue1 = fields[3],
                .clue2 = fields[4],
                .clue3 = fields[5],
                .surprisal = surprisal,
            }) catch @panic("append failed");
        }

        // Sort by surprisal (lowest to highest)
        std.mem.sort(Entry, entries.items, {}, struct {
            fn lessThan(_: void, lhs: Entry, rhs: Entry) bool {
                return lhs.surprisal < rhs.surprisal;
            }
        }.lessThan);

        // Write header file
        const header_file = root_dir.createFile(io, header_path, .{}) catch @panic("failed to create header file");
        var header_buf: [64 * 1024]u8 = undefined;
        var header_writer = header_file.writer(io, &header_buf);
        const hw = &header_writer.interface;
        hw.writeAll(
            \\#ifndef _CLUES_
            \\#define _CLUES_
            \\
            \\#include <stddef.h>
            \\
            \\typedef struct {
            \\    char *word;
            \\    size_t word_length;
            \\    char *category;
            \\    char *clues[3];
            \\    size_t clue_length[3];
            \\    double surprisal;
            \\} Word;
            \\
            \\static const Word words[] = {
            \\
        ) catch @panic("write failed");

        std.debug.print("Starting to write!\n", .{});
        for (entries.items) |e| {
            hw.print(
                \\    {{"{s}", {d}, "{s}", {{"{s}", "{s}", "{s}"}}, {{{d}, {d}, {d}}}, {d:.6}}},
                \\
            , .{
                e.word,      e.word.len,
                e.category,  e.clue1,
                e.clue2,     e.clue3,
                e.clue1.len, e.clue2.len,
                e.clue3.len, e.surprisal,
            }) catch @panic("write failed");
        }
        std.debug.print("Done writing!\n", .{});

        hw.writeAll(
            \\};
            \\
            \\static const size_t words_count = sizeof(words) / sizeof(words[0]);
            \\
            \\#endif
            \\
        ) catch @panic("write failed");
        hw.flush() catch @panic("flush failed");
        header_file.close(io);

        allocator.free(word_file);
    };

    ///////////////////////////////////////////////////////////////////////////
    // Embed assets into a header
    const assets_path = "src" ++ std.fs.path.sep_str ++ "assets.h";
    root_dir.access(io, assets_path, .{}) catch {
        const assets_file = root_dir.createFile(io, assets_path, .{}) catch @panic("failed to create assets.h");
        defer assets_file.close(io);

        var assets_buf: [64 * 1024]u8 = undefined;
        var assets_writer = assets_file.writer(io, &assets_buf);
        const aw = &assets_writer.interface;

        aw.writeAll(
            \\#ifndef _ASSETS_
            \\#define _ASSETS_
            \\
            \\
        ) catch @panic("write failed");

        const asset_files = [_]struct { name: []const u8, path: []const u8 }{
            .{ .name = "asset_type_wav", .path = "assets/type.wav" },
            .{ .name = "asset_solve_wav", .path = "assets/solve.wav" },
        };

        for (asset_files) |asset| {
            const data = root_dir.readFileAlloc(io, asset.path, std.heap.page_allocator, .limited(1024 * 1024)) catch @panic("failed to read asset");
            defer std.heap.page_allocator.free(data);

            aw.print("static const unsigned char {s}[] = {{\n", .{asset.name}) catch @panic("write");

            for (data, 0..) |byte, i| {
                aw.print("0x{x:0>2},", .{byte}) catch @panic("write");
                if ((i + 1) % 16 == 0) aw.writeAll("\n") catch @panic("write");
            }

            aw.writeAll("\n};\n") catch @panic("write");

            aw.print("static const int {s}_size = {d};\n\n", .{ asset.name, data.len }) catch @panic("write");
        }

        aw.writeAll("#endif\n") catch @panic("write");
        aw.flush() catch @panic("flush failed");
    };

    ///////////////////////////////////////////////////////////////////////////
    // Create the build
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "Crossword",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    // Link prebuilt raylib static library (deps/raylib-prebuilt)
    const resolved = exe.root_module.resolved_target.?.result;
    const is_macos = resolved.os.tag == .macos;
    const is_linux = resolved.os.tag == .linux;
    const is_windows = resolved.os.tag == .windows;

    exe.root_module.addIncludePath(b.path("deps/raylib-prebuilt"));

    if (is_macos) {
        exe.root_module.addObjectFile(b.path("deps/raylib-prebuilt/macos/libraylib.a"));
        exe.root_module.linkFramework("Cocoa", .{});
        exe.root_module.linkFramework("IOKit", .{});
        exe.root_module.linkFramework("CoreVideo", .{});
        exe.root_module.linkFramework("OpenGL", .{});
    } else if (is_linux) {
        exe.root_module.addObjectFile(b.path("deps/raylib-prebuilt/linux_amd64/libraylib.a"));
        exe.root_module.linkSystemLibrary("GL", .{});
        exe.root_module.linkSystemLibrary("X11", .{});
        exe.root_module.linkSystemLibrary("m", .{});
        exe.root_module.linkSystemLibrary("pthread", .{});
        exe.root_module.linkSystemLibrary("dl", .{});
        exe.root_module.linkSystemLibrary("rt", .{});
    } else if (is_windows) {
        exe.root_module.addObjectFile(b.path("deps/raylib-prebuilt/windows_amd64/libraylib.a"));
        exe.root_module.linkSystemLibrary("opengl32", .{});
        exe.root_module.linkSystemLibrary("gdi32", .{});
        exe.root_module.linkSystemLibrary("winmm", .{});
        exe.root_module.linkSystemLibrary("user32", .{});
        exe.root_module.linkSystemLibrary("shell32", .{});
    } else {
        @panic("Unsupported platform: only macOS, Linux, and Windows are supported");
    }

    const is_release = optimize != .Debug;
    const c_flags: []const []const u8 = if (is_release)
        &.{ "-std=c11", "-Wall", "-Wextra", "-pedantic", "-DMODE_PRODUCTION", "-D_POSIX_C_SOURCE=200809L" }
    else
        &.{ "-std=c11", "-Wall", "-Wextra", "-pedantic", "-D_POSIX_C_SOURCE=200809L" };

    exe.root_module.addIncludePath(b.path("deps/staunch/include"));
    addCSourceDir(b, exe.root_module, "deps/staunch/src", c_flags);

    exe.root_module.addIncludePath(b.path("src"));
    addCSourceDirExclude(b, exe.root_module, "src", c_flags, &.{"eval_main.c"});

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);
    b.step("run", "Run the game").dependOn(&run_cmd.step);

    ///////////////////////////////////////////////////////////////////////////
    // Eval executable (headless, no raylib)
    const eval_exe = b.addExecutable(.{
        .name = "eval",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    const eval_c_flags: []const []const u8 = if (is_release)
        &.{ "-std=c11", "-Wall", "-Wextra", "-pedantic", "-DMODE_PRODUCTION", "-DMODE_EVAL", "-D_POSIX_C_SOURCE=200809L" }
    else
        &.{ "-std=c11", "-Wall", "-Wextra", "-pedantic", "-DMODE_EVAL", "-D_POSIX_C_SOURCE=200809L" };

    eval_exe.root_module.addIncludePath(b.path("deps/staunch/include"));
    addCSourceDir(b, eval_exe.root_module, "deps/staunch/src", eval_c_flags);

    eval_exe.root_module.addIncludePath(b.path("src"));
    eval_exe.root_module.addCSourceFile(.{
        .file = b.path("src/eval_main.c"),
        .flags = eval_c_flags,
    });
    eval_exe.root_module.addCSourceFile(.{
        .file = b.path("src/crossword.c"),
        .flags = eval_c_flags,
    });

    b.installArtifact(eval_exe);

    const eval_run = b.addRunArtifact(eval_exe);
    eval_run.step.dependOn(b.getInstallStep());
    if (b.args) |args| eval_run.addArgs(args);
    b.step("eval", "Run the evaluation").dependOn(&eval_run.step);

    ///////////////////////////////////////////////////////////////////////////
    // Generate compile_commands.json for LSP (clangd / Zed)
    const compile_commands_path = "compile_commands.json";
    const compile_commands = root_dir.createFile(io, compile_commands_path, .{}) catch @panic("failed to create compile_commands.json");
    defer compile_commands.close(io);

    var cc_buf: [64 * 1024]u8 = undefined;
    var cc_file_writer = compile_commands.writer(io, &cc_buf);
    const cc_writer = &cc_file_writer.interface;

    cc_writer.writeAll("[\n") catch @panic("write failed");

    const root = b.build_root.path.?;
    const dirs_to_scan = [_][]const u8{ "deps/staunch/src", "src" };
    var first = true;
    for (dirs_to_scan) |dir_path| {
        const dir = root_dir.openDir(io, dir_path, .{ .iterate = true }) catch continue;
        defer dir.close(io);
        var iter = dir.iterate();
        while (iter.next(io) catch null) |entry| {
            if (entry.kind != .file or !std.mem.endsWith(u8, entry.name, ".c")) continue;
            if (!first) cc_writer.writeAll(",\n") catch @panic("write failed");
            first = false;
            cc_writer.print(
                \\  {{
                \\    "directory": "{s}",
                \\    "file": "{s}/{s}/{s}",
                \\    "command": "cc -std=c11 -Wall -Wextra -pedantic -D_POSIX_C_SOURCE=200809L -I{s}/deps/staunch/include -I{s}/src -I{s}/deps/raylib-prebuilt {s}/{s}/{s}"
                \\  }}
            , .{
                root,
                root,
                dir_path,
                entry.name,
                root,
                root,
                root,
                root,
                dir_path,
                entry.name,
            }) catch @panic("write failed");
        }
    }

    cc_writer.writeAll("\n]\n") catch @panic("write failed");
    cc_writer.flush() catch @panic("flush failed");
}

fn addCSourceDir(
    b: *std.Build,
    module: *std.Build.Module,
    dir_path: []const u8,
    c_flags: []const []const u8,
) void {
    addCSourceDirExclude(b, module, dir_path, c_flags, &.{});
}

fn addCSourceDirExclude(
    b: *std.Build,
    module: *std.Build.Module,
    dir_path: []const u8,
    c_flags: []const []const u8,
    exclude: []const []const u8,
) void {
    const io = b.graph.io;
    const dir = b.build_root.handle.openDir(io, dir_path, .{ .iterate = true }) catch {
        std.debug.print("failed to open directory: {s}\n", .{dir_path});
        @panic("build failed");
    };
    defer dir.close(io);

    var iter = dir.iterate();
    while (iter.next(io) catch null) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".c")) {
            var skip = false;
            for (exclude) |ex| {
                if (std.mem.eql(u8, entry.name, ex)) {
                    skip = true;
                    break;
                }
            }
            if (skip) continue;

            module.addCSourceFile(.{
                .file = b.path(b.fmt("{s}/{s}", .{ dir_path, entry.name })),
                .flags = c_flags,
            });
        }
    }
}
