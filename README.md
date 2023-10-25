# Tephra
**_A high-level C++17 graphics and computing library based on [Vulkan](https://www.vulkan.org/)_**

**License**: [MIT](https://github.com/Dolkar/Tephra/blob/main/LICENSE)

**Current version**: [v0.2.0](https://dolkar.github.io/Tephra/changelog.html)

**Links**: [User guide](https://dolkar.github.io/Tephra/user-guide.html) | [API Documentation](https://dolkar.github.io/Tephra/annotated.html) |
           [Discussions](https://github.com/Dolkar/Tephra/discussions)

**Build status**: ![build status](https://github.com/Dolkar/Tephra/actions/workflows/build.yml/badge.svg)

## About

Tephra aspires to provide a modern alternative to high-level graphics APIs like OpenGL and DirectX 11, while leveraging
the benefits of the underlying Vulkan ecosystem. Its goal is to strike a good balance between ease-of-use, performance
and relevance. To that end, Tephra provides:
- A high-level job system for submitting batches of work to the GPU (or to other accelerators that expose the Vulkan
  API)
- Low-level command lists that can be recorded in parallel and with minimal overhead
- An easy and efficient way to create temporary images, staging buffers and scratch memory
- A simple, general-purpose interface that tries not to force architectural decisions upon the user (such as recording
  callbacks, the concept of frames or a bindless resource model)
- The ability to use bleeding-edge features of graphics hardware through direct interoperability with the Vulkan API
- An introductory [user guide](https://dolkar.github.io/Tephra/user-guide.html), extensive
  [documentation](https://dolkar.github.io/Tephra/annotated.html) and [examples](https://dolkar.github.io/Tephra/examples.html)
  for getting started with using the library without prior knowledge of Vulkan
- Debugging features, usage validation and testing suite (WIP)

_Tephra is being used and partially developed by [Bohemia Interactive Simulations](https://bisimulations.com/)._

### Comparison to OpenGL / DirectX 11

One of the main differences when moving over from these older graphics APIs is the execution model. Much like in Vulkan,
your draw calls don't take effect immediately in Tephra, but are instead recorded into either jobs or command lists that
then get executed at a later time. There is no "immediate context". This allows for easy parallel recording and
full control over the execution of workloads. Recording commands is usually done in two passes. A "job" first defines
high-level commands such as:
- Allocation of temporary job-local resources
- Clears, copies, blits and resolves
- Render and compute passes specifying target resources and a set of command lists to execute
- Resource export commands and Vulkan interop commands

The actual draw and dispatch commands then get recorded into the command lists of each pass after the job itself has
been finalized. For convenience, a callback function can be optionally used to record a small command list in-place to
help with code organization.

While Tephra handles most of the Vulkan-mandated synchronization automatically from the list of job commands,
analyzing commands recorded into command lists would have unacceptable performance overhead. Instead, this ordinarily
needs to be handled by specifying all the resource accesses of each render / compute pass, but for the majority of
read-only accesses, the library offers the much more convenient "export" mechanism. Once an image or buffer is written
to by a prior command or pass, it can be exported for all future accesses of a certain type, for example as a sampled
texture. In effect, this means that you generally need to specify if and how a resource is going to be read from inside
your shaders in the future, after each time you write into it.

Another system inherited from Vulkan is its binding model. By default, resources get bound as descriptors in sets,
rather than individually. You can think of a material's textures - the albedo map, normal map, roughness map, etc -
as one descriptor set, through which all of its textures get bound to a compatible shader pipeline at the same time.
Alternatively, you can use the "bindless" style of managing a global array of all of your textures inside a single giant
descriptor set that you then index into inside your shaders. Tephra streamlines working with either method.
 
### Comparison to Vulkan and other Vulkan abstractions

Starting from the initialization stage, Tephra already provides amenities for interacting with the varied world of
Vulkan devices. Arbitrary number of queues can be used from each supported queue family, irrespective of the actual
number exposed by Vulkan. Feature maps and format utilities further help handle hardware differences. Vulkan profile
support is planned, making the process of choosing and relying upon a specific set of hardware features even easier.
Overall, the initialization process is greatly simplified compared to raw Vulkan, akin to using the
[vk-bootstrap](https://github.com/charles-lunarg/vk-bootstrap) library.

Tephra leverages [VMA](https://gpuopen.com/vulkan-memory-allocator/) for all of its resource allocations. On top of that,
it allows efficient use of temporary resources within each job. Requested job-local resources can be aliased to the same
memory location to reduce memory usage if their usage does not overlap. Growable ring buffers provide temporary staging
buffers for easy uploading of data. The pools that all these reusable resources are allocated from are controllable and
configurable. In general, the library tries to avoid allocations whenever possible, opting instead for pooling and reuse.

RAII is used to manage the lifetime of resources and other objects. Their destruction is delayed until the device is done
using the objects, so they can be safely dropped even right after enqueuing a job. The idea of buffer and image views
has been expanded upon and nearly all interactions with resources are done through these non-owning views. They can
reference the entire resource, or just its part, and are relatively cheap to create on the fly.

Automatic synchronization is implemented between all job commands submitted to the same queue. The implementation tries
to minimize the number of barriers without reordering the commands - the control of that is left in the hands of
the user. All dependencies are resolved on a subresource level, including byte ranges for buffers and array layers / mip
levels for images. Synchronization across different queues is handled with timeline semaphores and resource exports in
a thread safe manner.

Many other Vulkan abstractions opt for
[render graphs](https://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/) to manage synchronization
and resource aliasing. Tephra's approach is similar to a render graph that does not reorder passes, but has a smaller
API footprint, does not force resource virtualization and is already familiar to users of last-gen graphics APIs. A
render graph solution can be easily implemented on top of Tephra, if desired.

Descriptor sets differ from Vulkan's by being immutable. Changing them requires waiting until the device is done with
any workload that uses it, which is infeasible in practice. Instead, Tephra recycles and reuses old descriptor sets
to create new ones in the background. Besides these ordinary descriptor sets, a mutable descriptor set implementation is
also provided. It can be useful for emulating the binding of individual resources, or to assist with a bindless resource
model.

Tephra provides many other abstractions around Vulkan, such as pools, pipelines, swapchain and others to form an
all-encompassing, high-level-ish graphics library. You generally do not need to use the Vulkan API directly, except when
working with extensions that Tephra does not natively support, or when interacting with various device properties and
features.

## Feature list

The following features are already present:
- All of the compute and graphics commands supported by core Vulkan
- Automatic synchronization and resource state tracking inside and across queues
- Temporary resource allocator that leverages aliasing to reduce memory usage
- Support for multi-threaded recording and device-level thread safety
- Improved image and buffer views
- Safe delayed destruction of Vulkan handles
- Interoperability with plain Vulkan (WIP)
- Debug logging, statistics, tests and partial usage validation (WIP)

The following features are planned and will likely be available in the future:
- Timestamp, occlusion and pipeline queries
- Ray tracing features
- Better handling of dynamic pipeline state
- Improved pipeline building and management
- Native support of commonly used Vulkan extensions
- Vulkan profiles

The following features are out of scope for the library and won't be included:
- Platform-dependent window management - use GLFW or a similar library instead
- Shader compilation and reflection - use the existing Vulkan ecosystem
- Sparse buffers and images
- Linear images
- Rendering algorithms - this is not a renderer or a game engine

See the [user guide](https://dolkar.github.io/Tephra/user-guide.html) for more detailed explanations and inline code
examples of Tephra's features, or the [examples](https://github.com/Dolkar/Tephra/tree/main/examples) folder for a
runnable showcase.

## Prerequisities

- Tephra is a C++ library. It makes use of C++17 features, the standard library and C++ exceptions.
- Only the Visual Studio build path for the Windows platform is maintained, but the library itself is written to be
  platform independent.
- Vulkan headers version 1.3.239 or newer, provided with the SDK [here](https://www.lunarg.com/vulkan-sdk/)
- Compatible devices must support Vulkan 1.3 or newer, as well as the [timeline semaphore feature](https://vulkan.gpuinfo.org/listdevicescoverage.php?core=1.2&feature=timelineSemaphore&platform=all)

Building the documentation
- Python 3.6 or newer
- Doxygen 1.8.15 or newer
- The [Jinja2](https://palletsprojects.com/p/jinja/) and [Pygments](https://pygments.org/) Python packages

## Contributing

Feel free to create issues, submit pull requests for non-trivial changes and participate in
[Discussions](https://github.com/Dolkar/Tephra/discussions). Submitting examples, validation and tests is also very
appreciated.


