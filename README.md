# Tephra
**_A high-level C++17 graphics and computing library based on [Vulkan](https://www.vulkan.org/)_**

**License**: MIT [LINK]

**Current version**: v0.1 [LINK]

**Links**: User guide | Documentation | Discussions | Contributing

**Build status**: Windows, Documentation

## About

Tephra aspires to provide a modern alternative to high-level graphics APIs like OpenGL and DirectX 11, while leveraging the benefits of the underlying Vulkan ecosystem. Its goal is to strike a good balance between ease-of-use, performance and applicability. To that end, Tephra provides:
- A high-level job system for submitting batches of work to the GPU (or other accelerators that expose the Vulkan API)
- Low-level command lists that can be recorded in parallel and with minimal overhead
- An easy and efficient way to create temporary images, staging buffers and scratch memory
- A general-purpose interface that tries not to impose architectural decisions upon the user (such as render graphs, callbacks, or the concept of frames)
- The ability to use bleeding-edge features of graphics hardware through direct interoperability with the Vulkan API
- An introductory user guide [LINK] with examples for getting started with using the library without requiring prior knowledge of Vulkan.
- Extensive documentation [LINK] and debugging features

_Tephra is being used and partially developed by [Bohemia Interactive Simulations](https://bisimulations.com/)._

### Comparison to OpenGL / DirectX 11

One of the main differences when moving over from these older graphics APIs is the execution model. Much like in Vulkan, your draw calls don't take effect immediately in Tephra, but are first recorded into either jobs, or command lists, depending on the nature of the command. There is no "immediate context". This allows for easy parallel recording and full control over the execution of workloads. This recording is usually done in two passes. A "job" first defines high-level commands such as:
- Allocation of temporary job-local resources
- Clears, copies, blits and resolves
- Render and compute passes specifying target resources and a set of command lists to record
- Resource export commands and Vulkan interop commands

The actual draw and dispatch commands are then recorded into the command lists of each pass after the job itself has been recorded. For convenience, a callback function can be optionally used to record a small command list to help keep related code in the same place.

While Tephra can handle most of the Vulkan mandated synchronization automatically from the list of job commands, analyzing commands recorded into command lists would have unacceptable performance overhead. Instead, this is ordinarily handled by specifying all resource accesses in the render / compute passes, but for the majority of read-only accesses, the library offers the much more convenient "export" mechanism. Once an image or buffer is written to by a prior command or pass, it can be exported for all future accesses of a certain type, for example as a sampled texture. In effect, this means that you generally need to specify how a resource is going to be read after being written.

Another system inherited from Vulkan is its binding model. By default, resources get bound as descriptors in sets, rather than individually. You can think of a material's textures - the albedo map, normal map, roughness map, etc - as one descriptor set that gets bound to a compatible shader pipeline at the same time. Alternatively, you can use the "bindless" style of managing a global array of all of your textures inside a single giant descriptor set that you then index into inside your shaders. Tephra streamlines working with either method.
 
### Comparison to Vulkan and other Vulkan abstractions

Starting from the initialization stage, Tephra already provides amenities for interacting with the varied world of Vulkan devices. Arbitrary number of queues can be used from each supported queue family, irrespective of the actual number exposed by Vulkan. Feature maps and format utilities further help handle hardware differences. Tephra can also make use of multi-subpass render passes, which are important for mobile platforms, at least until [similar functionality](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_shader_tile_image.html) becomes prevalent.

Tephra leverages [VMA](https://gpuopen.com/vulkan-memory-allocator/) for all of its resource allocations. On top of that, it allows efficient use of temporary resources within each job. Requested job-local resources can be aliased to the same memory location to reduce memory usage if their usage does not overlap. Growable ring buffers serve temporary staging buffers for easy uploading of data. The pools that all these reusable resources are allocated from are controllable and configurable. In general, the library tries to avoid allocations whenever possible, opting instead for pooling and reuse. Also, any Tephra object can be destroyed without having to wait on the device to finish using it.

Automatic synchronization is implemented between all job commands submitted to the same queue. The implementation tries to minimize the number of barriers without reordering the commands - the control of that is left in the hands of the user. All dependencies are resolved on a subresource level, including byte ranges for buffers and array layers / mip levels for images. Synchronization across different queues is handled with timeline semaphores and resource exports in a thread safe manner.

Descriptor sets differ from Vulkan's by being immutable. Changing them requires waiting until the device is done with any workload that uses it, which is infeasible in practice. Instead, Tephra recycles and reuses old descriptor sets to create new ones. Besides the ordinary descriptor sets, a mutable descriptor set implementation is also provided. It can be useful for emulating the binding of individual resources, or to assist with a bindless resource model.

A lot of other smaller stuff. Vulkan symbols do not need to be used unless you're working with extensions.


## Prerequisities

## Feature list


## Other notes


