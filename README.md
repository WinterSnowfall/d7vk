# D7VK

A Vulkan-based translation layer for Direct3D 7, which allows running 3D applications on Linux using Wine. It uses DXVK's D3D9 backend as well as Wine's DDraw implementation (or the windows native DDraw) and acts as a proxy between the two, providing a minimal D3D7-on-D3D9 implementation.

## FAQ

### Will D7VK work with every D3D7 game out there?

Sadly, no. D3D7 is a land of highly cursed API interoperability, and applications that for one reason or another mix and match D3D7 with older DDraw (not DDraw7) and/or with GDI are not expected to ever work. If those games provide alternative renderers, based on Glide or OpenGL, I strongly recommend you use those, together with [nGlide](https://www.zeus-software.com/downloads/nglide) where applicable.

If you're wondering about the current state of a certain game, a good starting point would be checking [the issue tracker](https://github.com/WinterSnowfall/d7vk/issues).

### What should I do if a game doesn't work (properly) with D7VK?

I'll try to get as much game coverage as possible in D7VK, of course, but if something just doesn't work, simply use WineD3D - it's awesome and has the benefit of implementing *everything* there ever is to worry about in DDraw and GDI, so it's far, far less prone to cursed interop madness. Reports of issues and bugs are very welcome, as they ensure proper tracking and awareness, so please do report any problems you encounter if you have the time.

### Is D7VK really needed?

No, not really. I am aware there are plenty of other (good) options out there for D3D7 translation, and while D7VK may perform better in some applications/situations, it will most likely not outperform those other options universally. But having more options on the table is a good thing in my book at least.

You can also expect it to have the same level of per application/targeted configuration profiles and fixes that you're used to seeing in DXVK proper. It also gives us (D3D8/9 DXVK developers) a platform to stress test the fixed function implementation with even older games, which is one of the main goals of the project... besides me wanting to play Sacrifice and Disciples II on top of DXVK. Yeah, that's how it all started.

### Will DXVK's D3D9 config options work with D7VK?

Yes, because D7VK relies on DXVK's D3D9 backend, so everything ends up there anyway.

### VSync isn't turning off although the application lets me disable it. What gives?

VSync is universally enabled by default in D3D7, and thus also in D7VK. In fact, D3D7 devices have to explicitly expose support for being able to *turn off* VSync, since not all of them were (allegedly) capable of doing it back in the day. This is why the vast majority of D3D7 applications don't even bother with trying to change the defaults, and will implicitly enable VSync. In some cases, turning it off will simply not work reliably, even if an option is provided.

Note that D7VK does properly support turning it off in some cases, e.g. Unreal Tournament with the OldUnreal patch applied.

That being said, D7VK will also enforce various frame rate limits, provided as built-in config options, for games that are known to break or suffer from various bugs at high frame rates. These situations are very much an issue on high refresh rate displays, regardless of VSync.

You can, however, use the traditional DXVK config options for controlling either frame rate limits or the presentation interval (VSync), namely: `d3d9.maxFrameRate` and `d3d9.presentInterval`, with values of your choosing, either to override any existing settings or to specify your own. Doing so is most likely going to cause issues, unless some form of mod/modern patch resolves the underlying physics/input handling/rendering limitations that many of these applications were confronted with at high frame rates.

### Since D3D7 AA isn't actually supported, is there a way to force MSAA?

Yes, use `d3d7.forceMSAA = <your_desired_MSAA_level>`. 2, 4 and 8 (x MSAA) are supported. Note that D3D7 AA support is advertised, so games will let you enable it, however `D3DRENDERSTATE_ANTIALIAS`/`D3DRS_MULTISAMPLEANTIALIAS` isn't implemented in (D3D9) DXVK, so you will not get any AA without forcing the MSAA level.

### Will it work on Windows?

Maybe? I'm not using Windows, so can't test it or develop it to be adapted to such situations. Its primarily intended use case is, and always will be, Wine/Linux. To that end, D7VK is pretty much aligned with upstream DXVK.

### Will it be upstreamed to DXVK at some point?

No.

### Will it be expanded to include support for earlier D3D APIs?

Also no. D3D7 is enough of a challenge and a mess as it is. The further we stray from D3D9, the further we stray from the divine.

## Acknowledgments

None of this would have ever been possible without DXVK and Wine, so remember to show your love to the awesome people involved in those projects. A special thanks goes out to [AlpyneDreams](https://github.com/AlpyneDreams) (of D8VK fame), both for D8VK and also for providing a good reference experimental branch, without which I would not have even considered diving head first into spinning off D7VK.

## How to use
Grab the latest release or compile the project manually if you want to be "on the bleeding edge".

To give it a spin in a Wine prefix of choice, copy the `ddraw.dll` file next to the game/application executable, then open `winecfg` and manually add `native, builtin` (explicitly in that order) DLL overrides for `ddraw` under the Libraries tab. There's no need to worry about bitness or anything like that, since D3D7 has always been 32-bit exclusive.

On Windows, simply copying `ddraw.dll` next to the game executable should work just fine. Note that Windows use is largely untested and D7VK is primarily aimed at use with Wine/Linux, so your mileage may vary.

Do NOT, I repeat, do NOT copy `ddraw.dll` in your Wine or Windows system directories, as you will need access to an actual DDraw implementation for any of this to work.

Verify that your application uses D7VK instead of wined3d by enabling the HUD (see notes below).

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
When used with Wine, D7VK will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `DXVK_LOG_PATH` variable, where log files in the given directory will be called `app_d3d7.log` etc., where `app` is the name of the game executable.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

## Any other doubts?

Please refer to the upstream DXVK wiki and documentation, available [here](https://github.com/doitsujin/dxvk).

Feel free to report any issues you encounter, should they not already be present on the issue tracker.

