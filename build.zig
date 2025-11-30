const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const raylib_dep = b.dependency("raylib", .{ .target = target, .optimize = optimize });

    const exe = b.addExecutable(.{
        .name = "game-name-goes-here-please-update-it",
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
