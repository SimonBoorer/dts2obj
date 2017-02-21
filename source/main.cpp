//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include <stdio.h>

#include "platform/platform.h"
#include "platform/platformVolume.h"
#include "app/mainLoop.h"
#include "T3D/gameFunctions.h"
#include "core/stream/fileStream.h"
#include "core/resourceManager.h"
#include "ts/tsShape.h"
#include "ts/tsShapeConstruct.h"
#include "ts/tsMaterialList.h"
#include "materials/materialManager.h"
#include "gfx/bitmap/gBitmap.h"

#ifdef TORQUE_OS_WIN
#include "platformWin32/platformWin32.h"
#include "platformWin32/winConsole.h"
#endif

/** Print the usage string */
void printUsage()
{
	Con::printf(
		"DTS-2-OBJ Converter v%s (c) GarageGames, LLC.\n\n"
		"dts2obj [options] dtsFilename\n\n"
		"--output objFilename   Set the output OBJ filename.\n"
		"Exits with zero on success, non-zero on failure\n\n",
		TORQUE_APP_VERSION_STRING);
}

Torque::Path makeFullPath(const char* path)
{
	char tempBuf[1024];
	Platform::makeFullPathName(path, tempBuf, sizeof(tempBuf), Platform::getCurrentDirectory());
	return Torque::Path(String(tempBuf));
}

S32 TorqueMain( S32 argc, const char **argv )
{
	S32 failed = 0;

	// Initialize the subsystems.
	StandardMainLoop::init();
	Con::setVariable( "Con::Prompt", "" );
	WindowsConsole->enable( true );

	// install all drives for now until we have everything using the volume stuff
	Platform::FS::InstallFileSystems();
	Platform::FS::MountDefaults();

	Torque::Path srcPath, destPath;

	// Parse arguments
	S32 i;
	for (i = 1; i < argc - 1; i++)
	{
		if (dStrEqual(argv[i], "--output"))
			destPath = makeFullPath(argv[++i]);
	}

	if ((i >= argc) || (!dStrEndsWith(argv[i], ".dts")))
	{
		Con::errorf("Error: no DTS file specified.\n");
		printUsage();

		// Clean everything up.
		StandardMainLoop::shutdown();

		return -1;
	}

	srcPath = makeFullPath(argv[i]);
	if (destPath.isEmpty())
	{
		destPath = srcPath;
		destPath.setExtension("obj");
	}

	TSShape tss;
	TSShape::smInitOnRead = false;

	// Attempt to load the DTS file
	FileStream srcStream;
	srcStream.open(srcPath, Torque::FS::File::Read);
	if (srcStream.getStatus() != Stream::Ok)
	{
		Con::errorf("Failed to convert DTS file: %s\n", srcPath.getFullPath());
		failed = 1;
	}
	else
	{
		tss.read(&srcStream);
		srcStream.close();

		FileStream destStream;
		destStream.open(destPath, Torque::FS::File::Write);
		if (destStream.getStatus() != Stream::Ok)
		{
			Con::errorf("Failed to save shape to '%s'", destPath.getFullPath().c_str());
			failed = 1;
		}
		else
		{
			const Vector<String>& materialNames = tss.materialList->getMaterialNameList();

			Torque::Path mtlPath(destPath);
			mtlPath.setExtension("mtl");

			FileStream mtlStream;
			mtlStream.open(mtlPath, Torque::FS::File::Write);
			if (mtlStream.getStatus() != Stream::Ok)
			{
				Con::errorf("Failed to save materials to '%s'", mtlPath.getFullPath().c_str());
				failed = 1;
			}
			else
			{
				S32 numMaterials = materialNames.size();
				for (S32 i = 0; i < numMaterials; i++)
				{
					String materialName = materialNames[i];

					Torque::Path path;
					if (GBitmap::sFindFile(materialName, &path))
					{
						String materialFileName = path.getFullFileName();

						mtlStream.writeFormattedBuffer("newmtl %s\r\n", materialName.c_str());
						mtlStream.writeFormattedBuffer("Ka 1.000 1.000 1.000\r\n");
						mtlStream.writeFormattedBuffer("Kd 1.000 1.000 1.000\r\n");
						mtlStream.writeFormattedBuffer("map_Ka %s\r\n", materialFileName.c_str());
						mtlStream.writeFormattedBuffer("map_Kd %s\r\n", materialFileName.c_str());
					}
				}

				mtlStream.close();
				destStream.writeFormattedBuffer("mtllib %s\r\n", mtlPath.getFullFileName().c_str());
			}
	
			S32 vertOffset = 1;
			S32 numObjects = tss.objects.size();
			for (S32 i = 0; i < numObjects; i++)
			{
				TSShape::Object object = tss.objects[i];

				String name = tss.names[object.nameIndex];
				destStream.writeFormattedBuffer("g %s\r\n", name.c_str());

				MatrixF mat;
				tss.getNodeWorldTransform(object.nodeIndex, &mat);

				for (S32 j = object.startMeshIndex; j < object.startMeshIndex + object.numMeshes; j++)
				{
					TSMesh *mesh = tss.meshes[j];
					if (mesh == NULL)
					{
						continue;
					}

					S32 numVerts = mesh->verts.size();
					for (S32 k = 0; k < numVerts; k++)
					{
						Point3F vert = mesh->verts[k];
						mat.mulP(vert);

						destStream.writeFormattedBuffer("v %f %f %f\r\n", vert.x, vert.y, vert.z);
					}

					S32 numTVerts = mesh->tverts.size();
					for (S32 k = 0; k < numTVerts; k++)
					{
						Point2F tvert = mesh->tverts[k];
						tvert.y = 1.0f - tvert.y;

						destStream.writeFormattedBuffer("vt %f %f\r\n", tvert.x, tvert.y);
					}

					S32 numNorms = mesh->norms.size();
					for (S32 k = 0; k < numNorms; k++)
					{
						Point3F norm = mesh->norms[k];
						destStream.writeFormattedBuffer("vn %f %f %f\r\n", norm.x, norm.y, norm.z);
					}

					S32 numPrimitives = mesh->primitives.size();
					for (S32 k = 0; k < numPrimitives; k++)
					{
						TSDrawPrimitive primitive = mesh->primitives[k];

						if (!(primitive.matIndex & TSDrawPrimitive::NoMaterial))
						{
							U32 material = primitive.matIndex & TSDrawPrimitive::MaterialMask;

							String materialName = materialNames[material];
							destStream.writeFormattedBuffer("usemtl %s\r\n", materialName.c_str());
						}

						if ((primitive.matIndex & TSDrawPrimitive::TypeMask) == TSDrawPrimitive::Triangles)
						{
							for (S32 l = primitive.start; l < primitive.start + primitive.numElements; l += 3)
							{
								U32 idx0 = mesh->indices[l + 0];
								U32 idx1 = mesh->indices[l + 1];
								U32 idx2 = mesh->indices[l + 2];

								destStream.writeFormattedBuffer("f %d/%d/%d %d/%d/%d %d/%d/%d\r\n",
									idx0 + vertOffset, idx0 + vertOffset, idx0 + vertOffset,
									idx1 + vertOffset, idx1 + vertOffset, idx1 + vertOffset,
									idx2 + vertOffset, idx2 + vertOffset, idx2 + vertOffset);
							}
						}
						else
						{
							U32 idx0 = mesh->indices[primitive.start + 0];
							U32 idx1;
							U32 idx2 = mesh->indices[primitive.start + 1];
							U32* nextIdx = &idx1;
							for (S32 l = 2; l < primitive.numElements; l++)
							{
								*nextIdx = idx2;
								nextIdx = (U32*)((dsize_t)nextIdx ^ (dsize_t)&idx0 ^ (dsize_t)&idx1);
								idx2 = mesh->indices[primitive.start + l];
								if (idx0 == idx1 || idx1 == idx2 || idx2 == idx0)
									continue;

								destStream.writeFormattedBuffer("f %d/%d/%d %d/%d/%d %d/%d/%d\r\n",
									idx0 + vertOffset, idx0 + vertOffset, idx0 + vertOffset,
									idx1 + vertOffset, idx1 + vertOffset, idx1 + vertOffset,
									idx2 + vertOffset, idx2 + vertOffset, idx2 + vertOffset);
							}
						}
					}

					vertOffset += numVerts;
				}
			}

			destStream.close();
		}
	}

	// Clean everything up.
	StandardMainLoop::shutdown();

	// Do we need to restart?
	if( StandardMainLoop::requiresRestart() )
		Platform::restartInstance();

	return failed;
}
