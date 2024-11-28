#include "material.h"

#include "application.h"

#include <istream>
#include <fstream>
#include <algorithm>

enum ShaderType {
	ABSORPTION_SHADER,
	BASIC_SHADER,
	NORMAL_SHADER,
	EMISSION_ABSORPTION,
	SCATTERING_SHADER
};
enum VolumeType {
	HOMOGENEOUS = 0,
	HETEROGENEOUS = 1
};

enum DensityType {
	CONSTANT = 0,
	NOISE = 1,
	TEXTURE = 2
};

// store the current shader selection
ShaderType currentShaderType = SCATTERING_SHADER;
VolumeType currentVolumeType = HOMOGENEOUS;
DensityType currentDensityType = CONSTANT;

FlatMaterial::FlatMaterial(glm::vec4 color)
{
	this->color = color;
	this->shader = Shader::Get("res/shaders/basic.vs", "res/shaders/flat.fs");
}

FlatMaterial::~FlatMaterial() { }

void FlatMaterial::setUniforms(Camera* camera, glm::mat4 model)
{
	//upload node uniforms
	this->shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	this->shader->setUniform("u_camera_position", camera->eye);
	this->shader->setUniform("u_model", model);

	this->shader->setUniform("u_color", this->color);
}

void FlatMaterial::render(Mesh* mesh, glm::mat4 model, Camera* camera)
{
	if (mesh && this->shader) {
		// enable shader
		this->shader->enable();

		// upload uniforms
		setUniforms(camera, model);

		// do the draw call
		mesh->render(GL_TRIANGLES);

		this->shader->disable();
	}
}

void FlatMaterial::renderInMenu()
{
	ImGui::ColorEdit3("Color", (float*)&this->color);
}

WireframeMaterial::WireframeMaterial()
{
	this->color = glm::vec4(1.f);
	this->shader = Shader::Get("res/shaders/basic.vs", "res/shaders/flat.fs");
}

WireframeMaterial::~WireframeMaterial() { }

void WireframeMaterial::render(Mesh* mesh, glm::mat4 model, Camera* camera)
{
	if (this->shader && mesh)
	{
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);

		//enable shader
		this->shader->enable();

		//upload material specific uniforms
		setUniforms(camera, model);

		//do the draw call
		mesh->render(GL_TRIANGLES);

		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
}

StandardMaterial::StandardMaterial(glm::vec4 color)
{
	this->color = color;
	this->base_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/basic.fs");
	this->normal_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/normal.fs");
	this->shader = this->base_shader;
}

StandardMaterial::~StandardMaterial() { }

void StandardMaterial::setUniforms(Camera* camera, glm::mat4 model)
{
	//upload node uniforms
	this->shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	this->shader->setUniform("u_camera_position", camera->eye);
	this->shader->setUniform("u_model", model);

	this->shader->setUniform("u_color", this->color);

	if (this->texture) {
		this->shader->setUniform("u_texture", this->texture);
	}
}

void StandardMaterial::render(Mesh* mesh, glm::mat4 model, Camera* camera)
{
	bool first_pass = true;
	if (mesh && this->shader)
	{
		// enable shader
		this->shader->enable();

		// Multi pass render
		int num_lights = Application::instance->light_list.size();
		for (int nlight = -1; nlight < num_lights; nlight++)
		{
			if (nlight == -1) { nlight++; } // hotfix

			// upload uniforms
			setUniforms(camera, model);

			// upload light uniforms
			if (!first_pass) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glDepthFunc(GL_LEQUAL);
			}
			this->shader->setUniform("u_ambient_light", Application::instance->ambient_light * (float)first_pass);

			if (num_lights > 0) {
				Light* light = Application::instance->light_list[nlight];
				light->setUniforms(this->shader, model);
			}
			else {
				// Set some uniforms in case there is no light
				this->shader->setUniform("u_light_intensity", 1.f);
				this->shader->setUniform("u_light_shininess", 1.f);
				this->shader->setUniform("u_light_color", glm::vec4(0.f));
			}

			// do the draw call
			mesh->render(GL_TRIANGLES);
            
			first_pass = false;
		}

		// disable shader
		this->shader->disable();
	}
}

void StandardMaterial::renderInMenu()
{
	if (ImGui::Checkbox("Show Normals", &this->show_normals)) {
		if (this->show_normals) {
			this->shader = this->normal_shader;
		}
		else {
			this->shader = this->base_shader;
		}
	}

	if (!this->show_normals) ImGui::ColorEdit3("Color", (float*)&this->color);
}

VolumeMaterial::VolumeMaterial(double absorption_coefficient, glm::vec4 color, float noise_scale, int noise_detail, float step_length, float emission_coefficient, float density_scale, float scattering_coefficient, float isotropy_parameter)
{
	this->color = color;
	this->absorption_coefficient = absorption_coefficient;
	this->noise_scale = noise_scale;
	this->noise_detail = noise_detail;
	this->step_length = step_length;
	this->emission_coefficient = emission_coefficient;
	this->density_scale = density_scale;
	this->max_light_steps = 100;
	this->basic_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/basic.fs");
	this->absorption_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/absorption.fs");
	this->normal_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/normal.fs");
	this->emission_absorption = Shader::Get("res/shaders/basic.vs", "res/shaders/emission-absorption.fs");
	this->scattering_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/scattering.fs");
	this->shader = this->scattering_shader;
	this->scattering_coefficient = scattering_coefficient;
	this->isotropy_parameter = isotropy_parameter;
	this->jittering_offset = false;

}

void VolumeMaterial::setUniforms(Camera* camera, glm::mat4 model, Mesh* mesh) {
	//Convert Camera position to Local Coordinates
	glm::mat4 inverseModel = glm::inverse(model);
	glm::vec4 temp = glm::vec4(camera->eye, 1.0);
	temp = inverseModel * temp;
	glm::vec3 local_camera_pos = glm::vec3(temp.x / temp.w, temp.y / temp.w, temp.z / temp.w);

	glm::vec3 boxMin = mesh->aabb_min;
	glm::vec3 boxMax = mesh->aabb_max;

	this->shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	this->shader->setUniform("u_camera_position", local_camera_pos);
	this->shader->setUniform("u_model", model);

	this->shader->setUniform("u_color", this->color);
	this->shader->setUniform("u_absorption_coefficient", this->absorption_coefficient);
	this->shader->setUniform("u_emission_coefficient", this->emission_coefficient);
	this->shader->setUniform("u_scattering_coefficient", this->scattering_coefficient);

	this->shader->setUniform("u_boxMin", mesh->aabb_min);
	this->shader->setUniform("u_boxMax", mesh->aabb_max);


	this->shader->setUniform("u_ambient_light", Application::instance->ambient_light);
	this->shader->setUniform("u_background_color", Application::instance->background_color);

	int volumeTypeInt = static_cast<int>(currentVolumeType);
	this->shader->setUniform("u_volume_type", volumeTypeInt);
	this->shader->setUniform("u_step_length", this->step_length);
	this->shader->setUniform("u_noise_scale", this->noise_scale);
	this->shader->setUniform("u_noise_detail", this->noise_detail);
	this->shader->setUniform("u_max_light_steps", this->max_light_steps);

	// VDB-related uniforms
	if (this->texture) {
		this->shader->setUniform("u_density_texture", this->texture,0);
	}
	this->shader->setUniform("u_density_source", currentDensityType); // 0, 1, or 2 based on GUI selection
	this->shader->setUniform("u_density_scale", this->density_scale);
	this->shader->setUniform("u_constant_density", 1.f);
	this->shader->setUniform("u_isotropy_parameter", this->isotropy_parameter);

	this->shader->setUniform("u_jittering", this->jittering_offset);

	// Light uniforms
	//this->shader->setUniform("u_light_intensity");
	//this->shader->setUniform("u_light_color");
	//this->shader->setUniform("u_light_direction")
	//this->shader->setUniform("u_light_position");
}

void VolumeMaterial::render(Mesh* mesh, glm::mat4 model, Camera* camera) {
	bool first_pass = true;
	if (mesh && this->shader)
	{
		// enable shader
		this->shader->enable();

		// Multi pass render
		int num_lights = Application::instance->light_list.size();
		for (int nlight = -1; nlight < num_lights; nlight++)
		{
			if (nlight == -1) { nlight++; } // hotfix

			// upload uniforms
			setUniforms(camera, model, mesh);

			// upload light uniforms
			if (!first_pass) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glDepthFunc(GL_LEQUAL);
			}
			//this->shader->setUniform("u_ambient_light", Application::instance->ambient_light * (float)first_pass);
			//this->shader->setUniform("u_background_color", Application::instance->background_color);

			if (num_lights > 0) {
				Light* light = Application::instance->light_list[nlight];
				light->setUniforms(this->shader, model);
			}
			else {
				// Set some uniforms in case there is no light
				this->shader->setUniform("u_light_intensity", 1.f);
				this->shader->setUniform("u_light_shininess", 1.f);
				this->shader->setUniform("u_light_color", glm::vec4(0.f));
			}

			// do the draw call
			mesh->render(GL_TRIANGLES);

			first_pass = false;
		}

		// disable shader
		this->shader->disable();
	}
}

void VolumeMaterial::renderInMenu() {
	ImGui::Checkbox("Jittering Offset", &this->jittering_offset);
	ImGui::ColorEdit3("Color", (float*)&this->color);
	ImGui::SliderFloat("Absorption Coefficient", &this->absorption_coefficient, 0.0f, 2.0f); // Absorption control
	ImGui::SliderFloat("Scattering Coefficient", &this->scattering_coefficient, 0.0f, 2.0f); // Absorption control
	ImGui::SliderFloat("Step Length", &this->step_length, 0.01f, 3.0f); // Absorption control
	ImGui::SliderInt("Light Step Length", &this->max_light_steps, 1, 100);
	ImGui::SliderFloat("G parameter Value", &this->isotropy_parameter, -1.f, 1.0f);

	const char* shaderNames[] = { "Absorption Shader", "Basic Shader", "Normal Shader", "Emission-Absorption", "Scattering Shader"};
	const char* volumeTypeNames[] = { "Homogeneous", "Heterogeneous" };
	const char* densitySources[] = { "Constant", "3D Noise", "VDB File" };
	int shaderIndex = static_cast<int>(currentShaderType); 
	int volumeTypeIndex = static_cast<int>(currentVolumeType); 
	int densityIndex = static_cast<int>(currentDensityType);

	if (ImGui::Combo("Density Type", &densityIndex, densitySources, IM_ARRAYSIZE(densitySources))) {
		currentDensityType = static_cast<DensityType>(densityIndex);
	}
	ImGui::SliderFloat("Density Scale", &this->density_scale, 0.1f, 10.0f);

	if (ImGui::Combo("Shader Type", &shaderIndex, shaderNames, IM_ARRAYSIZE(shaderNames))) {
		currentShaderType = static_cast<ShaderType>(shaderIndex);
		setShader();
	}
	if (ImGui::Combo("Volume Type", &volumeTypeIndex, volumeTypeNames, IM_ARRAYSIZE(volumeTypeNames))) {
		currentVolumeType = static_cast<VolumeType>(volumeTypeIndex); 
	}


	//if (static_cast<int>(currentVolumeType) == 1) {
	//	ImGui::SliderFloat("Emission Coefficient", &this->emission_coefficient, 0.0f, 5.0f);

	//	//ImGui::SliderFloat("Noise Detail", &this->noise_detail, 0.0f, 5.0f); // Absorption control
	//	ImGui::SliderInt("Noise Detail", &this->noise_detail, 0, 5);
	//	ImGui::SliderFloat("Noise Scale", &this->noise_scale, 0.0f, 5.0f);
	//}

}

void VolumeMaterial::setShader() {
	switch (currentShaderType) {
	case ABSORPTION_SHADER:
		this->shader = this->absorption_shader;
		break;
	case BASIC_SHADER:
		this->shader = this->basic_shader;
		break;
	case NORMAL_SHADER:
		this->shader = this->normal_shader;
		break;
	case EMISSION_ABSORPTION:
		this->shader = this->emission_absorption;
		break;
	}
}

void VolumeMaterial::loadVDB(std::string file_path)
{
	easyVDB::OpenVDBReader* vdbReader = new easyVDB::OpenVDBReader();
	vdbReader->read(file_path);

	// now, read the grid from the vdbReader and store the data in a 3D texture
	estimate3DTexture(vdbReader);
}

void VolumeMaterial::estimate3DTexture(easyVDB::OpenVDBReader* vdbReader)
{
	int resolution = 128;
	float radius = 2.0;

	int convertedGrids = 0;
	int convertedVoxels = 0;

	int totalGrids = vdbReader->gridsSize;
	int totalVoxels = totalGrids * pow(resolution, 3);

	float resolutionInv = 1.0f / resolution;
	int resolutionPow2 = pow(resolution, 2);
	int resolutionPow3 = pow(resolution, 3);

	// read all grids data and convert to texture
	for (unsigned int i = 0; i < totalGrids; i++) {
		easyVDB::Grid& grid = vdbReader->grids[i];
		float* data = new float[resolutionPow3];
		memset(data, 0, sizeof(float) * resolutionPow3);

		// Bbox
		easyVDB::Bbox bbox = easyVDB::Bbox();
		bbox = grid.getPreciseWorldBbox();
		glm::vec3 target = bbox.getCenter();
		glm::vec3 size = bbox.getSize();
		glm::vec3 step = size * resolutionInv;

		grid.transform->applyInverseTransformMap(step);
		target = target - (size * 0.5f);
		grid.transform->applyInverseTransformMap(target);
		target = target + (step * 0.5f);

		int x = 0;
		int y = 0;
		int z = 0;

		for (unsigned int j = 0; j < resolutionPow3; j++) {
			int baseX = x;
			int baseY = y;
			int baseZ = z;
			int baseIndex = baseX + baseY * resolution + baseZ * resolutionPow2;

			if (target.x >= 40 && target.y >= 40.33 && target.z >= 10.36) {
				int a = 0;
			}

			float value = grid.getValue(target);

			int cellBleed = radius;

			if (cellBleed) {
				for (int sx = -cellBleed; sx < cellBleed; sx++) {
					for (int sy = -cellBleed; sy < cellBleed; sy++) {
						for (int sz = -cellBleed; sz < cellBleed; sz++) {
							if (x + sx < 0.0 || x + sx >= resolution ||
								y + sy < 0.0 || y + sy >= resolution ||
								z + sz < 0.0 || z + sz >= resolution) {
								continue;
							}

							int targetIndex = baseIndex + sx + sy * resolution + sz * resolutionPow2;

							float offset = std::max(0.0, std::min(1.0, 1.0 - std::hypot(sx, sy, sz) / (radius / 2.0)));
							float dataValue = offset * value * 255.f;

							data[targetIndex] += dataValue;
							data[targetIndex] = std::min((float)data[targetIndex], 255.f);
						}
					}
				}
			}
			else {
				float dataValue = value * 255.f;

				data[baseIndex] += dataValue;
				data[baseIndex] = std::min((float)data[baseIndex], 255.f);
			}

			convertedVoxels++;

			if (z >= resolution) {
				break;
			}

			x++;
			target.x += step.x;

			if (x >= resolution) {
				x = 0;
				target.x -= step.x * resolution;

				y++;
				target.y += step.y;
			}

			if (y >= resolution) {
				y = 0;
				target.y -= step.y * resolution;

				z++;
				target.z += step.z;
			}

			// yield
		}

		// now we create the texture with the data
		// use this: https://www.khronos.org/opengl/wiki/OpenGL_Type
		// and this: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage3D.xhtml
		this->texture = new Texture();
		this->texture->create3D(resolution, resolution, resolution, GL_RED, GL_FLOAT, false, data, GL_R8);
	}
}

IsoMaterial::IsoMaterial(double absorption_coefficient, glm::vec4 color, float noise_scale, int noise_detail, float step_length, float emission_coefficient, float density_scale, float scattering_coefficient, float isotropy_parameter)
{
	this->color = color;
	this->absorption_coefficient = absorption_coefficient;
	this->noise_scale = noise_scale;
	this->noise_detail = noise_detail;
	this->step_length = step_length;
	this->emission_coefficient = emission_coefficient;
	this->density_scale = density_scale;
	this->max_light_steps = 100;
	this->shader = Shader::Get("res/shaders/basic.vs", "res/shaders/isosurface.fs");
	this->scattering_coefficient = scattering_coefficient;
	this->isotropy_parameter = isotropy_parameter;
	this->jittering_offset = false;

}

void IsoMaterial::setUniforms(Camera* camera, glm::mat4 model, Mesh* mesh)
{
	//Convert Camera position to Local Coordinates
	glm::mat4 inverseModel = glm::inverse(model);
	glm::vec4 temp = glm::vec4(camera->eye, 1.0);
	temp = inverseModel * temp;
	glm::vec3 local_camera_pos = glm::vec3(temp.x / temp.w, temp.y / temp.w, temp.z / temp.w);

	glm::vec3 boxMin = mesh->aabb_min;
	glm::vec3 boxMax = mesh->aabb_max;

	this->shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	this->shader->setUniform("u_camera_position", local_camera_pos);
	this->shader->setUniform("u_model", model);

	this->shader->setUniform("u_color", this->color);
	this->shader->setUniform("u_absorption_coefficient", this->absorption_coefficient);
	this->shader->setUniform("u_emission_coefficient", this->emission_coefficient);
	this->shader->setUniform("u_scattering_coefficient", this->scattering_coefficient);

	this->shader->setUniform("u_boxMin", mesh->aabb_min);
	this->shader->setUniform("u_boxMax", mesh->aabb_max);


	this->shader->setUniform("u_ambient_light", Application::instance->ambient_light);
	this->shader->setUniform("u_background_color", Application::instance->background_color);

	int volumeTypeInt = static_cast<int>(currentVolumeType);
	this->shader->setUniform("u_volume_type", volumeTypeInt);
	this->shader->setUniform("u_step_length", this->step_length);
	this->shader->setUniform("u_noise_scale", this->noise_scale);
	this->shader->setUniform("u_noise_detail", this->noise_detail);
	this->shader->setUniform("u_max_light_steps", this->max_light_steps);

	// VDB-related uniforms
	if (this->texture) {
		this->shader->setUniform("u_density_texture", this->texture, 0);
	}
	this->shader->setUniform("u_density_scale", this->density_scale);
	this->shader->setUniform("u_constant_density", 1.f);
	this->shader->setUniform("u_isotropy_parameter", this->isotropy_parameter);

	this->shader->setUniform("u_jittering", this->jittering_offset);
}

void IsoMaterial::render(Mesh* mesh, glm::mat4 model, Camera* camera)
{
	bool first_pass = true;
	if (mesh && this->shader)
	{
		// enable shader
		this->shader->enable();

		// Multi pass render
		int num_lights = Application::instance->light_list.size();
		for (int nlight = -1; nlight < num_lights; nlight++)
		{
			if (nlight == -1) { nlight++; } // hotfix

			// upload uniforms
			setUniforms(camera, model, mesh);

			// upload light uniforms
			if (!first_pass) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				glDepthFunc(GL_LEQUAL);
			}
			//this->shader->setUniform("u_ambient_light", Application::instance->ambient_light * (float)first_pass);
			//this->shader->setUniform("u_background_color", Application::instance->background_color);

			if (num_lights > 0) {
				Light* light = Application::instance->light_list[nlight];
				light->setUniforms(this->shader, model);
			}
			else {
				// Set some uniforms in case there is no light
				this->shader->setUniform("u_light_intensity", 1.f);
				this->shader->setUniform("u_light_shininess", 1.f);
				this->shader->setUniform("u_light_color", glm::vec4(0.f));
			}

			// do the draw call
			mesh->render(GL_TRIANGLES);

			first_pass = false;
		}

		// disable shader
		this->shader->disable();
	}
}

void IsoMaterial::renderInMenu()
{
	ImGui::Checkbox("Jittering Offset", &this->jittering_offset);
	ImGui::ColorEdit3("Color", (float*)&this->color);
	ImGui::SliderFloat("Absorption Coefficient", &this->absorption_coefficient, 0.0f, 2.0f); // Absorption control
	ImGui::SliderFloat("Scattering Coefficient", &this->scattering_coefficient, 0.0f, 2.0f); // Absorption control
	ImGui::SliderFloat("Step Length", &this->step_length, 0.01f, 3.0f); // Absorption control
	//ImGui::SliderInt("Light Step Length", &this->max_light_steps, 1, 100);
	ImGui::SliderFloat("G parameter Value", &this->isotropy_parameter, -1.f, 1.0f);

	ImGui::SliderFloat("Density Scale", &this->density_scale, 0.1f, 10.0f);

}

void IsoMaterial::setShader()
{
}

void IsoMaterial::loadVDB(std::string file_path)
{
	easyVDB::OpenVDBReader* vdbReader = new easyVDB::OpenVDBReader();
	vdbReader->read(file_path);

	// now, read the grid from the vdbReader and store the data in a 3D texture
	estimate3DTexture(vdbReader);
}

void IsoMaterial::estimate3DTexture(easyVDB::OpenVDBReader* vdbReader)
{
	int resolution = 128;
	float radius = 2.0;

	int convertedGrids = 0;
	int convertedVoxels = 0;

	int totalGrids = vdbReader->gridsSize;
	int totalVoxels = totalGrids * pow(resolution, 3);

	float resolutionInv = 1.0f / resolution;
	int resolutionPow2 = pow(resolution, 2);
	int resolutionPow3 = pow(resolution, 3);

	// read all grids data and convert to texture
	for (unsigned int i = 0; i < totalGrids; i++) {
		easyVDB::Grid& grid = vdbReader->grids[i];
		float* data = new float[resolutionPow3];
		memset(data, 0, sizeof(float) * resolutionPow3);

		// Bbox
		easyVDB::Bbox bbox = easyVDB::Bbox();
		bbox = grid.getPreciseWorldBbox();
		glm::vec3 target = bbox.getCenter();
		glm::vec3 size = bbox.getSize();
		glm::vec3 step = size * resolutionInv;

		grid.transform->applyInverseTransformMap(step);
		target = target - (size * 0.5f);
		grid.transform->applyInverseTransformMap(target);
		target = target + (step * 0.5f);

		int x = 0;
		int y = 0;
		int z = 0;

		for (unsigned int j = 0; j < resolutionPow3; j++) {
			int baseX = x;
			int baseY = y;
			int baseZ = z;
			int baseIndex = baseX + baseY * resolution + baseZ * resolutionPow2;

			if (target.x >= 40 && target.y >= 40.33 && target.z >= 10.36) {
				int a = 0;
			}

			float value = grid.getValue(target);

			int cellBleed = radius;

			if (cellBleed) {
				for (int sx = -cellBleed; sx < cellBleed; sx++) {
					for (int sy = -cellBleed; sy < cellBleed; sy++) {
						for (int sz = -cellBleed; sz < cellBleed; sz++) {
							if (x + sx < 0.0 || x + sx >= resolution ||
								y + sy < 0.0 || y + sy >= resolution ||
								z + sz < 0.0 || z + sz >= resolution) {
								continue;
							}

							int targetIndex = baseIndex + sx + sy * resolution + sz * resolutionPow2;

							float offset = std::max(0.0, std::min(1.0, 1.0 - std::hypot(sx, sy, sz) / (radius / 2.0)));
							float dataValue = offset * value * 255.f;

							data[targetIndex] += dataValue;
							data[targetIndex] = std::min((float)data[targetIndex], 255.f);
						}
					}
				}
			}
			else {
				float dataValue = value * 255.f;

				data[baseIndex] += dataValue;
				data[baseIndex] = std::min((float)data[baseIndex], 255.f);
			}

			convertedVoxels++;

			if (z >= resolution) {
				break;
			}

			x++;
			target.x += step.x;

			if (x >= resolution) {
				x = 0;
				target.x -= step.x * resolution;

				y++;
				target.y += step.y;
			}

			if (y >= resolution) {
				y = 0;
				target.y -= step.y * resolution;

				z++;
				target.z += step.z;
			}

			// yield
		}

		// now we create the texture with the data
		// use this: https://www.khronos.org/opengl/wiki/OpenGL_Type
		// and this: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexImage3D.xhtml
		this->texture = new Texture();
		this->texture->create3D(resolution, resolution, resolution, GL_RED, GL_FLOAT, false, data, GL_R8);
	}
}