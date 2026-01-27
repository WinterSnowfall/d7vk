# D7VK

A Vulkan-based translation layer for Direct3D 7, 6 and 5 which allows running 3D applications on Linux using Wine. It uses DXVK's D3D9 backend as well as Wine's DDraw implementation (or the windows native DDraw) and acts as a proxy between the two, providing a minimal D3D7/6/5-on-D3D9 implementation.

Note however that D3D6/5 games making use of the legacy D3D3 rendering pipeline (using execute buffers) are not supported by D7VK. Needless to say, neither are any cursed D3D retained-mode applications, since the project only implements immediate-mode.

## FAQ

### Will D7VK work with every game out there?

Sadly, no. DDraw and older D3D is a land of highly cursed API interoperability, and applications that for one reason or another mix and match D3D7/6/5 with DDraw and/or with GDI are not expected to work properly in all cases. If games provide alternative renderers, based on Glide or OpenGL, I strongly recommend you use those, together with [nGlide](https://www.zeus-software.com/downloads/nglide) where applicable.

If you're wondering about the current state of a certain game, a good starting point would be checking [the issue tracker](https://github.com/WinterSnowfall/d7vk/issues).

### Wait, what? There's D3D6/5 support in my D7VK! How did it get there?

After looking over the D3D6/5 SDK documentation, they turned out to be somewhat approachable, so I have implemented both. Support for D3D7 still remains the main goal of the project, but support for D3D6/5 will also be provided, as an experimental addition. From a features and general compatibility standpoint, expect them to fare somewhat worse than D3D7, because, as I've said before: the further we stray from D3D8/9, the further we stray from the divine.

### Why not spin off a D5VK and a D6VK, or rename the project?

All APIs prior to D3D8 fall under the cursed umbrella of DDraw, so it makes absolutely no sense to split things up. As for any renaming, that won't happen, since the project's main focus remains unchanged.

### Does that mean you'll add support for D3D3 at some point?

No. But we fully support D3D4 already, heh. Jokes aside, I have looked over earlier API documentation as well, and I can confidently say that while D3D6/5 are still reasonably similar to D3D9, D3D3 uses a much, much cruder rendering pipeline that simply isn't worth mapping onto DXVK's D3D9 backend.

It also makes very little sense to consider it, especially given its complexity, since there's very little hardware acceleration to speak of before D3D6/5 (and even in D3D6/5, if we're to be honest). You're more than fine with good ol' software rendering.

### What should I do if a game doesn't work (properly) with D7VK?

I'll try to get as much game coverage as possible in D7VK, of course, but if something just doesn't work, simply use WineD3D - it's awesome and has the benefit of implementing *everything* there ever is to worry about in DDraw and GDI, so it's far, far less prone to cursed interop madness. Reports of issues and bugs are very welcome, as they ensure proper tracking and awareness, so please do report any problems you encounter if you have the time.

### Will DXVK's D3D9 config options work with D7VK?

Yes, because D7VK relies on DXVK's D3D9 backend, so everything ends up there anyway.

### VSync isn't turning off although the application lets me disable it. What gives?

VSync is universally enabled by default with older D3D, and thus also in D7VK. In fact, older D3D devices have to explicitly expose support for being able to *turn off* VSync, since not all of them were (allegedly) capable of doing it back in the day. This is why the vast majority of applications don't even bother with trying to change the defaults, and will implicitly enable VSync. In some cases, turning it off will simply not work reliably, even if an option is provided.

Note that D7VK does properly support turning it off in some cases, e.g. Unreal Tournament with the OldUnreal patch applied.

That being said, D7VK will also enforce various frame rate limits, provided as built-in config options, for games that are known to break or suffer from various bugs at high frame rates. These situations are very much an issue on high refresh rate displays, regardless of VSync.

You can, however, use the traditional DXVK config options for controlling either frame rate limits or the presentation interval (VSync), namely: `d3d9.maxFrameRate` and `d3d9.presentInterval`, with values of your choosing, either to override any existing settings or to specify your own. Be warned that doing so is most likely going to cause issues, unless some form of mod/modern patch resolves the underlying physics/input handling/rendering limitations that many of these applications were confronted with at high frame rates.

### Is there a way to force enable AA?

Yes, use `d3d7.emulateFSAA = Forced`. Note that FSAA emulation is supported by D7VK, and some applications will outright provide you with the means to enable or disable it. Only use the above config option if you want to force enable AA, regardless of application support. Please also keep in mind that force enabling AA may not work well in all cases, and screen edge artifacting and/or GUI element corruption are possible consequences.

Should you encounter any situation in which AA support is listed as unavailable / greyed out by an application (without it being forced, as per the above), please raise an issue on our tracker.

### Do the D3D7 config options work for D3D6/5?

Yes. As do any of DVXK's D3D9 config options, just like they do for D3D7.

### Is D7VK really needed?

No, not really. I am aware there are plenty of other (good) options out there for older D3D translation, and while D7VK may perform better in some applications/situations, it will most likely not outperform those other options universally. But having more options on the table is a good thing in my book at least.

You can also expect it to have the same level of per application/targeted configuration profiles and fixes that you're used to seeing in DXVK proper. It also gives us (D3D8/9 DXVK developers) a platform to stress test the fixed function implementation with even older games, which is one of the main goals of the project... besides me wanting to play Sacrifice and Disciples II on top of DXVK. Yeah, that's how it all started.

### Will it work on Windows?

Maybe? I'm not using Windows, so can't test it or develop it to be adapted to such situations. Its primarily intended use case is, and always will be, Wine/Linux. To that end, D7VK is pretty much aligned with upstream DXVK.

### Will it be upstreamed to DXVK at some point?

No.

### Why not? Just do it!

Because DXVK's development team have made it clear they are not interested in merging and/or maintaining anything prior to D3D8. Also, considering this project takes a minimal approach in its DDraw implementation, it operates on a different principle compared to mainline DXVK, which is why it's best kept as a separate project altogether. I understand the desire to forge the One Ring, ehm... have things unified, but in this case it simply isn't meant to be.

## Acknowledgments

None of this would have ever been possible without DXVK and Wine, so remember to show your love to the awesome people involved in those projects. A special thanks goes out to [AlpyneDreams](https://github.com/AlpyneDreams) (of D8VK fame), both for D8VK and also for providing a good reference experimental branch, without which I would not have even considered diving head first into spinning off D7VK.

## How to use
Grab the latest release or compile the project manually if you want to be "on the bleeding edge".

To give it a spin in a Wine prefix of choice, copy the `ddraw.dll` file next to the game/application executable, then open `winecfg` and manually add `native, builtin` (explicitly in that order) DLL overrides for `ddraw` under the Libraries tab. There's no need to worry about bitness or anything like that, since DDraw (along with its D3D companions) has always been 32-bit exclusive.

On Windows, simply copying `ddraw.dll` next to the game executable should work just fine. Note that Windows use is largely untested and D7VK is primarily aimed at use with Wine/Linux, so your mileage may vary.

Do NOT, I repeat, do NOT copy `ddraw.dll` in your Windows system directories, as you will need access to an actual DDraw implementation for any of this to work, and that might also break your system.

When using Wine on Linux, there also is an alternate deployment option, required by some games such as _GTA 2_, _StarLancer_, _Midtown Madness 2_ and others, namely renaming the system path Wine `ddraw.dll` file to `ddraw_.dll` and copying `ddraw.dll` in your `system32` or `syswow64` system path directly (DO mind your prefix bitness in this case!). D7VK will prioritize loading the `ddraw_.dll` file from its current path before trying to load `ddraw.dll` from the system path, in order to accommodate both methods of use. Note that you will need to set up proper dll overrides in Wine even in this case, as described above.

Verify that your application uses D7VK instead of wined3d by enabling the HUD (see notes below).

#### DLL dependencies
Listed below are the DLL requirements for using DXVK with any single API.

- d3d7: `ddraw.dll`
- d3d6: `ddraw.dll`
- d3d5: `ddraw.dll`

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
When used with Wine, D7VK will print log messages to `stderr`. Additionally, standalone log files can optionally be generated by setting the `D7VK_LOG_PATH` variable, where log files in the given directory will be called `app_ddraw.log` etc., where `app` is the name of the game executable.

Note that you need to use the `D7VK_LOG_LEVEL` variable to control logging verbosity. The naming of the environment variables has been altered in order to allow for finer control of logging specifically for D7VK, independently of upstream DXVK.

On Windows, log files will be created in the game's working directory by default, which is usually next to the game executable.

## Any other doubts?

Please refer to the upstream DXVK wiki and documentation, available [here](https://github.com/doitsujin/dxvk).

Feel free to report any issues you encounter, should they not already be present on the issue tracker.

