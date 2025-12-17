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
    ///////////////////////////////////////////////////////////////////////////
    // create the clue dataset
    const header_path = "src" ++ std.fs.path.sep_str ++ "clues.h";
    std.fs.cwd().access(header_path, .{}) catch {
        var gpa = std.heap.GeneralPurposeAllocator(.{}){};
        defer _ = gpa.deinit();
        const allocator = gpa.allocator();

        const word_path = "data" ++ std.fs.path.sep_str ++ "clues.csv";
        const word_file = std.fs.cwd().readFileAlloc(allocator, word_path, 10 * 1024 * 1024) catch @panic("failed to create word file");

        var entries: std.ArrayListUnmanaged(Entry) = .{};
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
            defer allocator.free(word_upper);

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
        var header_file = std.fs.cwd().createFile(header_path, .{}) catch @panic("failed to create header file");

        header_file.writeAll(
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

        for (entries.items) |e| {
            var buf: [2048]u8 = undefined;
            const formatted = std.fmt.bufPrint(&buf,
                \\    {{"{s}", {d}, "{s}", {{"{s}", "{s}", "{s}"}}, {{{d}, {d}, {d}}}, {d:.6}}},
                \\
            , .{
                e.word,      e.word.len,
                e.category,  e.clue1,
                e.clue2,     e.clue3,
                e.clue1.len, e.clue2.len,
                e.clue3.len, e.surprisal,
            }) catch @panic("format failed");

            header_file.writeAll(formatted) catch @panic("write failed");
        }

        header_file.writeAll(
            \\};
            \\
            \\static const size_t words_count = sizeof(words) / sizeof(words[0]);
            \\
            \\#endif
            \\
        ) catch @panic("write failed");

        header_file.close();
        allocator.free(word_file);
    };

    ///////////////////////////////////////////////////////////////////////////
    // Create the build
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const raylib_dep = b.dependency("raylib", .{ .target = target, .optimize = optimize });

    const exe = b.addExecutable(.{
        .name = "Crossword",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    exe.root_module.addIncludePath(raylib_dep.path("src"));

    const is_release = optimize != .Debug;
    const c_flags: []const []const u8 = if (is_release)
        &.{ "-std=c99", "-Wall", "-Wextra", "-pedantic", "-DMODE_PRODUCTION" }
    else
        &.{ "-std=c99", "-Wall", "-Wextra", "-pedantic" };

    includeDir(b, exe.root_module, "deps/staunch/Glow", c_flags);
    includeDir(b, exe.root_module, "deps/staunch/Exam", c_flags);
    includeDir(b, exe.root_module, "deps/staunch/Foundation", c_flags);
    includeDir(b, exe.root_module, "src", c_flags);

    exe.root_module.linkLibrary(raylib_dep.artifact("raylib"));

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);
    b.step("run", "Run the game").dependOn(&run_cmd.step);
}

fn includeDir(
    b: *std.Build,
    module: *std.Build.Module,
    dir_path: []const u8,
    c_flags: []const []const u8,
) void {
    module.addIncludePath(b.path(dir_path));

    var dir = b.build_root.handle.openDir(dir_path, .{ .iterate = true }) catch {
        std.debug.print("failed to open directory: {s}\n", .{dir_path});
        @panic("build failed");
    };
    defer dir.close();

    var iter = dir.iterate();
    while (iter.next() catch null) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".c")) {
            module.addCSourceFile(.{
                .file = b.path(b.fmt("{s}/{s}", .{ dir_path, entry.name })),
                .flags = c_flags,
            });
        }
    }
}
