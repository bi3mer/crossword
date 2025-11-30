const std = @import("std");

pub fn build(b: *std.Build) void {
    // create the clue dataset
    const header_path = "src" ++ std.fs.path.sep_str ++ "clues.h";
    std.fs.cwd().access(header_path, .{}) catch {
        var gpa = std.heap.GeneralPurposeAllocator(.{}){};
        defer _ = gpa.deinit();
        const allocator = gpa.allocator();

        const word_path = "data" ++ std.fs.path.sep_str ++ "clues.csv";
        const word_file = std.fs.cwd().readFileAlloc(allocator, word_path, 10 * 1024 * 1024) catch @panic("failed to create word file");

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
                    // Strip quotes if present
                    if (field.len >= 2 and field[0] == '"' and field[field.len - 1] == '"') {
                        field = field[1 .. field.len - 1];
                    }
                    fields[field_idx] = field;
                    field_idx += 1;
                    field_start = i + 1;
                }
            }
            // Last field
            if (field_idx < 6) {
                var field = line[field_start..];
                if (field.len >= 2 and field[0] == '"' and field[field.len - 1] == '"') {
                    field = field[1 .. field.len - 1];
                }
                fields[field_idx] = field;
            }

            const word = fields[0];
            const surprisal_str = fields[1];
            const category = fields[2];
            const clue1 = fields[3];
            const clue2 = fields[4];
            const clue3 = fields[5];

            const surprisal = -@log(std.fmt.parseFloat(f64, surprisal_str) catch 0.0);

            var buf: [2048]u8 = undefined;
            const formatted = std.fmt.bufPrint(&buf,
                \\    {{"{s}", {d}, "{s}", {{"{s}", "{s}", "{s}"}}, {{{d}, {d}, {d}}}, {d:.6}}},
                \\
            , .{
                word,      word.len,
                category,  clue1,
                clue2,     clue3,
                clue1.len, clue2.len,
                clue3.len, surprisal,
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

    if (b.build_root.handle.openDir("src", .{ .iterate = true })) |dir| {
        var d = dir;
        defer d.close();
        var iter = d.iterate();
        while (iter.next() catch null) |entry| {
            if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".c")) {
                exe.root_module.addCSourceFile(.{
                    .file = b.path(b.fmt("src/{s}", .{entry.name})),
                    .flags = &.{ "-std=c99", "-Wall", "-Wextra", "-pedantic" },
                });
            }
        }
    } else |_| {}

    exe.root_module.linkLibrary(raylib_dep.artifact("raylib"));

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| run_cmd.addArgs(args);
    b.step("run", "Run the game").dependOn(&run_cmd.step);
}
