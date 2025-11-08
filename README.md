# D7VK

A Vulkan-based translation layer for Direct3D 7, which allows running 3D applications on Linux using Wine. It uses DXVK's d3d9 backend as well as Wine's ddraw implementation (or the windows native ddraw) and acts as a proxy between the two, providing a minimal d3d7-on-d3d9 implementation. The project is currently in its early days. Expect most things to run, but not necessarily correctly or optimally.

## FAQ

### Will d7vk work with every d3d7 game out there?

Sadly, no. d3d7 is a land of highly cursed API inter-operability, and applications that for one reason or another mix and match d3d7 with older ddraw (not ddraw7) and/or with GDI are not expected to ever work. If those games provide alternative renderers, based on Glide or OpenGL, I strongly recommend you use those, together with [nGlide](https://www.zeus-software.com/downloads/nglide) where applicable.

If you're wondering about the current state of a certain game, a good starting point would be checking [the issue tracker](https://github.com/WinterSnowfall/d7vk/issues).

### Is d7vk really needed?

No, not really. I am aware there are plenty of other (good) options out there for d3d7 translation, and while d7vk may perform better in some applications/situations, it will most likely not outperform those other options universally. But having more options on the table is a good thing in my book at least.

You can also expect it to have the same level of per application/targeted configuration profiles and fixes that you're used to seeing in dxvk proper. It also gives us (d3d8/9 dxvk developers) a platform to stress test the fixed function implementation with even older games, which is one of the main goals of the project... besides me wanting to play Sacrifice and Disciples II on top of dxvk. Yeah, that's how it all started.

### Will dxvk's d3d9 config options work with d7vk?

Yes, because d7vk relies on dxvk's d3d9 backend, so everything ends up there anyway.

### Since d3d7 AA isn't actually supported, is there a way to force MSAA?

Yes, use `d3d7.forceMSAA = <your_desired_MSAA_level>`. 2, 4 and 8 (x MSAA) are supported. Note that d3d7 AA support is advertised, so games will let you enable it, however D3DRENDERSTATE_ANTIALIAS (toggleable AA) isn't compatible with d3d9, so you will not get any AA without forcing the MSAA level.

### Will it work on Windows?

Maybe? I'm not using Windows, so can't test it or develop it to be adapted to such situations. Its primarily intended use case is, and always will be, Wine/Linux. To that end, d7vk is pretty much aligned with upstream dxvk.

### Will it be upstreamed to dxvk at some point?

No.

### Will it be expanded to include support for earlier D3D APIs?

Also no. d3d7 is enough of a challenge and a mess as it is. The further we stray from d3d9, the further we stray from the divine.

## Acknowledgments

None of this would have ever been possible without DXVK and Wine, so remember to show your love to the awesome people involved in those projects. A special thanks goes out to [AlpyneDreams](https://github.com/AlpyneDreams) (of D8VK fame), both for D8VK and also for providing a good reference experimental branch, without which I would not have even considered diving head first into spinning off D7VK.

## How to use
Grab the latest release or compile the project manually if you want to be "on the bleeding edge".

To give it a spin in a Wine prefix of choice, copy the `ddraw.dll` file next to the game/application executable, then open `winecfg` and manually add `native, builtin` (explicitly in that order) DLL overrides for `ddraw` under the Libraries tab. There's no need to worry about bitness or anything like that, since d3d7 has always been 32-bit exclusive.

On Windows, simply copying `ddraw.dll` next to the game executable should work just fine. Note that Windows use is largely untested and d7vk is primarily aimed at use with Wine/Linux, so your mileage may vary.

Do NOT, I repeat, do NOT copy `ddraw.dll` in your Wine or Windows system directories, as you will need access to an actual ddraw implementation for any of this to work.

Verify that your application uses d7vk instead of wined3d by enabling the HUD (see notes below).

#### DLL dependencies
Listed below are the DLL requirements for using DXVK with any single API.

- d3d7: `ddraw.dll`

### HUD
The `DXVK_HUD` environment variable controls a HUD which can display the framerate and some stat counters. It accepts a comma-separated list of the following options:
- `devinfo`: Displays the name of the GPU and the driver version.
- `fps`: Shows the current frame rate.
- `frametimes`: Shows a frame time graph.
- `submissions`: Shows the number of command buffers submitted per frame.
- `drawcalls`: Shows the number of draw calls and render passes per frame.
- `pipelines`: Shows the total number of graphics and compute pipelines.
- `descriptors`: Shows the number of descriptor pools and descriptor sets.
- `memory`: Shows the amount of device memory allocated and used.
- `allocations`: Shows detailed memory chunk suballocation info.
- `gpuload`: Shows estimated GPU load. May be inaccurate.
- `version`: Shows DXVK version.
- `api`: Shows the D3D feature level used by the application.
- `cs`: Shows worker thread statistics.
- `compiler`: Shows shader compiler activity
- `samplers`: Shows the current number of sampler pairs used
- `ffshaders`: Shows the current number of shaders generated from fixed function state
- `swvp`: Shows whether or not the device is running in software vertex processing mode
- `scale=x`: Scales the HUD by a factor of `x` (e.g. `1.5`)
- `opacity=y`: Adjusts the HUD opacity by a factor of `y` (e.g. `0.5`, `1.0` being fully opaque).

Additionally, `DXVK_HUD=1` has the same effect as `DXVK_HUD=devinfo,fps`, and `DXVK_HUD=full` enables all available HUD elements.

### Logs
When used with Wine, d7vk will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `DXVK_LOG_PATH` variable, where log files in the given directory will be called `app_d3d7.log` etc., where `app` is the name of the game executable.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

## Any other doubts?

Please refer to the upstream DXVK wiki and documentation, available [here](https://github.com/doitsujin/dxvk).

Feel free to report any issues you encounter, should they not already be present on the issue tracker.

