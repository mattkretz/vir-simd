# dist directory

Definitions for distribution tools.

## RPM spec

Currently, there's an `rpkg` spec in here. If you'd like to use it locally, install `rpkg` on your fedoraoid of choice, and run

```shell
cd path/to/vir-simd
rpkg local --spec dist/vir-simd.spec
```

The more useful thing that can be done with this is that you can create a
[COPR](https://copr.fedorainfracloud.org), add a package of source type "SCM",
point it to your fork of vir-simd, specify "dist" as subdirectory, and use it
to build packages automatically; there's also webhooks integration, if you want
the package to be automatically rebuild on source repo forge events (usually,
when pushing a new tag).
