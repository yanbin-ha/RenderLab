#include "DirectIllumRaster.h"

#include <CppUtil/Engine/Scene.h>
#include <CppUtil/Engine/SObj.h>

#include <CppUtil/Engine/CmptGeometry.h>
#include <CppUtil/Engine/CmptMaterial.h>
#include <CppUtil/Engine/CmptTransform.h>

#include <CppUtil/Engine/Sphere.h>
#include <CppUtil/Engine/Plane.h>
#include <CppUtil/Engine/TriMesh.h>
#include <CppUtil/Engine/AllBSDFs.h>

#include <CppUtil/Engine/Light.h>
#include <CppUtil/Engine/AreaLight.h>
#include <CppUtil/Engine/PointLight.h>

#include <CppUtil/Qt/RawAPI_OGLW.h>
#include <CppUtil/Qt/RawAPI_Define.h>

#include <CppUtil/OpenGL/VAO.h>
#include <CppUtil/OpenGL/CommonDefine.h>
#include <CppUtil/OpenGL/Texture.h>
#include <CppUtil/OpenGL/Shader.h>
#include <CppUtil/OpenGL/Camera.h>
#include <CppUtil/OpenGL/FBO.h>

#include <CppUtil/Basic/LambdaOp.h>
#include <CppUtil/Basic/Sphere.h>
#include <CppUtil/Basic/Plane.h>
#include <CppUtil/Basic/Image.h>

#include <ROOT_PATH.h>

using namespace CppUtil::Engine;
using namespace CppUtil::QT;
using namespace CppUtil::OpenGL;
using namespace CppUtil::Basic;
using namespace CppUtil;
using namespace Define;
using namespace std;

const string rootPath = ROOT_PATH;

void DirectIllumRaster::Draw() {
	RasterBase::Draw();
}

void DirectIllumRaster::OGL_Init() {
	RasterBase::OGL_Init();

	InitShaders();
}

void DirectIllumRaster::InitShaders() {
	InitShaderBasic();
	InitShaderDiffuse();
	InitShaderMetalWorkflow();
	InitShaderFrostedGlass();
}

void DirectIllumRaster::InitShaderBasic() {
	shader_basic = Shader(rootPath + str_Basic_P3_vs, rootPath + str_Basic_fs);

	BindBlock(shader_basic);
}

void DirectIllumRaster::InitShaderDiffuse() {
	shader_diffuse = Shader(rootPath + str_Basic_P3N3T2_vs, rootPath + "data/shaders/Engine/BSDF_Diffuse.fs");
	
	BindBlock(shader_diffuse);

	shader_diffuse.SetInt("bsdf.albedoTexture", 0);

	SetShaderForShadow(shader_diffuse, 1);
}

void DirectIllumRaster::InitShaderMetalWorkflow() {
	shader_metalWorkflow = Shader(rootPath + str_Basic_P3N3T2T3_vs, rootPath + "data/shaders/Engine/BSDF_MetalWorkflow.fs");

	BindBlock(shader_metalWorkflow);

	shader_metalWorkflow.SetInt("bsdf.albedoTexture", 0);
	shader_metalWorkflow.SetInt("bsdf.metallicTexture", 1);
	shader_metalWorkflow.SetInt("bsdf.roughnessTexture", 2);
	shader_metalWorkflow.SetInt("bsdf.aoTexture", 3);
	shader_metalWorkflow.SetInt("bsdf.normalTexture", 4);

	SetShaderForShadow(shader_metalWorkflow, 5);
}

void DirectIllumRaster::InitShaderFrostedGlass() {
	shader_frostedGlass = Shader(rootPath + str_Basic_P3N3T2T3_vs, rootPath + "data/shaders/Engine/BSDF_FrostedGlass.fs");

	BindBlock(shader_frostedGlass);

	shader_frostedGlass.SetInt("bsdf.colorTexture", 0);
	shader_frostedGlass.SetInt("bsdf.roughnessTexture", 1);
	shader_frostedGlass.SetInt("bsdf.aoTexture", 2);
	shader_frostedGlass.SetInt("bsdf.normalTexture", 3);
	
	SetShaderForShadow(shader_frostedGlass, 4);
}

void DirectIllumRaster::Visit(Ptr<BSDF_Diffuse> bsdf) {
	SetCurShader(shader_diffuse);

	string strBSDF = "bsdf.";
	shader_diffuse.SetVec3f(strBSDF + "colorFactor", bsdf->colorFactor);
	if (bsdf->albedoTexture && bsdf->albedoTexture->IsValid()) {
		shader_diffuse.SetBool(strBSDF + "haveAlbedoTexture", true);
		GetTex(bsdf->albedoTexture).Use(0);
	}
	else
		shader_diffuse.SetBool(strBSDF + "haveAlbedoTexture", false);

	SetPointLightDepthMap(shader_diffuse, 1);
}

void DirectIllumRaster::Visit(Ptr<BSDF_Glass> bsdf) {
	SetCurShader(shader_basic);

	shader_basic.SetVec3f("color", bsdf->transmittance);
}

void DirectIllumRaster::Visit(Ptr<BSDF_Mirror> bsdf) {
	SetCurShader(shader_basic);

	shader_basic.SetVec3f("color", bsdf->reflectance);
}

void DirectIllumRaster::Visit(Ptr<BSDF_Emission> bsdf) {
	SetCurShader(shader_basic);

	shader_basic.SetVec3f("color", bsdf->GetEmission());
}

void DirectIllumRaster::Visit(Ptr<BSDF_CookTorrance> bsdf) {
	SetCurShader(shader_basic);

	shader_basic.SetVec3f("color", bsdf->refletance);
}

void DirectIllumRaster::Visit(Ptr<BSDF_MetalWorkflow> bsdf) {
	SetCurShader(shader_metalWorkflow);

	string strBSDF = "bsdf.";
	shader_metalWorkflow.SetVec3f(strBSDF + "colorFactor", bsdf->colorFactor);
	shader_metalWorkflow.SetFloat(strBSDF + "metallicFactor", bsdf->metallicFactor);
	shader_metalWorkflow.SetFloat(strBSDF + "roughnessFactor", bsdf->roughnessFactor);

	const int texNum = 5;
	PtrC<Image> imgs[texNum] = { bsdf->albedoTexture, bsdf->metallicTexture, bsdf->roughnessTexture, bsdf->aoTexture, bsdf->normalTexture };
	string names[texNum] = { "Albedo", "Metallic", "Roughness", "AO", "Normal" };

	for (int i = 0; i < texNum; i++) {
		string wholeName = strBSDF + "have" + names[i] + "Texture";
		if (imgs[i] && imgs[i]->IsValid()) {
			shader_metalWorkflow.SetBool(wholeName, true);
			GetTex(imgs[i]).Use(i);
		}
		else
			shader_metalWorkflow.SetBool(wholeName, false);
	}

	SetPointLightDepthMap(shader_metalWorkflow, texNum);
}

void DirectIllumRaster::Visit(Ptr<BSDF_FrostedGlass> bsdf) {
	SetCurShader(shader_frostedGlass);

	string strBSDF = "bsdf.";
	shader_frostedGlass.SetVec3f(strBSDF + "colorFactor", bsdf->colorFactor);
	shader_frostedGlass.SetFloat(strBSDF + "roughnessFactor", bsdf->roughnessFactor);

	const int texNum = 4;
	PtrC<Image> imgs[texNum] = { bsdf->colorTexture, bsdf->roughnessTexture, bsdf->aoTexture, bsdf->normalTexture };
	string names[texNum] = { "Color", "Roughness", "AO", "Normal" };

	for (int i = 0; i < texNum; i++) {
		string wholeName = strBSDF + "have" + names[i] + "Texture";
		if (imgs[i] && imgs[i]->IsValid()) {
			shader_frostedGlass.SetBool(wholeName, true);
			GetTex(imgs[i]).Use(i);
		}
		else
			shader_frostedGlass.SetBool(wholeName, false);
	}

	shader_frostedGlass.SetFloat(strBSDF + "ior", bsdf->ior);

	SetPointLightDepthMap(shader_frostedGlass, texNum);
}
