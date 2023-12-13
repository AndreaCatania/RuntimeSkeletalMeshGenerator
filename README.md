# This Fork fixes 5 major bugs in this plugin
The original version of the plugin has an issue that causes it to completely trash the model being loaded into UE4/5, this bug is fixed here.
And a second issue that cause animations to fail, this bug is fixed here.

See a sample on how to use it to create a procedural skeletal mesh and bones here: https://github.com/Fransferdy/creaturecreator/tree/main

# Runtime Skeletal Mesh Generator for UE5
Helper to create a SkeletalMeshComponent in UE5 at runtime.

This is a UE4 plugin that simplify the process of creating a `USkeletalMeshComponent`, with many surfaces, at runtime.
You can just pass all the surfaces' data, this library will take care to correctly populate the UE4 buffers, needed to have a fully working `USkeletalMeshComponent`.

## How to use it

To use this library:
1. Add this plugin inside the UE5 game plugins folder.
2. Specify `RuntimeSkeletalMeshGenerator` as plugin on your `Game.uproject`, to enable it.
3. Import the plugin using `#include "RuntimeSkeletalMeshGenerator/RuntimeSkeletalMeshGenerator.h"`

The plugin is ready to be used, here an example on how to use it:
```c++
#include "RuntimeSkeletalMeshGenerator/RuntimeSkeletalMeshGenerator.h"

void YourAmazingFunction()
{
	/// ----
	/// Populates the passed `SkeletalMesh` with the passed surface data.
	/// Make sure to pass a valid `USkeletalMesh`.
	FRuntimeSkeletalMeshGenerator::GenerateSkeletalMesh(
		SkeletalMesh,
		Surfaces,
		SurfacesMaterial,
		BoneTransformsOverride);



	/// ----
	/// Creates a new `USkeletalMesh` and a new `USkeletalMeshComponent`.
	/// It returns a valid `USkeletalMeshComponent`, with the generated `USkeletalMesh` assigned.
	FRuntimeSkeletalMeshGenerator::GenerateSkeletalMeshComponent(
		ActorOwner,
		Skeleton,
		Surfaces,
		SurfacesMaterial,
		BoneTransformsOverride);



	/// ----
	/// Creates a new `USkeletalMesh` and assign it to the given `SkeletalMeshComponent`.
	FRuntimeSkeletalMeshGenerator::UpdateSkeletalMeshComponent(
		SkeletalMeshComponent,
		Skeleton,
		Surfaces,
		SurfacesMaterial,
		BoneTransformsOverride);



	/// ----
	/// Decompose a `USkeletalMesh` to obtain the surfaces array.
	/// This API can be thought as the complement of the above
	/// three one, by submitting these arrays in one of the three
	/// functions you would obtain a new `USkeletalMesh` that
	/// would render the same mesh.

	TArray<FMeshSurface> OutSurfaces;                  // The out Surfaces data that compose the `USkeletalMesh`.
	TArray<int32> OutSurfacesVertexOffsets;            // The vertex offsets for each surface, relative to the passed `SkeletalMesh`: This is useful just in case you need to reconstruct the vertex index used on the `USkeletalMesh`.
	TArray<int32> OutSurfacesIndexOffsets;             // The index offsets for each surface, relative to the passed `SkeletalMesh`: This is useful just in case you need to reconstruct the vertex index used on the `USkeletalMesh`.
	TArray<UMaterialInterface*> OutSurfacesMaterial;   // The Materials used.
	
	FRuntimeSkeletalMeshGenerator::DecomposeSkeletalMesh(
		SkeletalMesh,
		TArray<FMeshSurface>& OutSurfaces,
		TArray<int32>& OutSurfacesVertexOffsets,
		TArray<int32>& OutSurfacesIndexOffsets,
		TArray<UMaterialInterface*>& OutSurfacesMaterial);
}
```

## Support

If you need any help, please post a question on the [Discussions page](https://github.com/AndreaCatania/RuntimeSkeletalMeshGenerator/discussions); and if you find a bug please consider to report it on the [Issues page](https://github.com/AndreaCatania/RuntimeSkeletalMeshGenerator/issues)

---

Authors:
- https://github.com/RevoluPowered
- https://github.com/AndreaCatania

License MIT

Kindly sponsored by IMVU
