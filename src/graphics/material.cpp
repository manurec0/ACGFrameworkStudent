#include "material.h"

#include "application.h"

#include <istream>
#include <fstream>
#include <algorithm>

enum ShaderType {
	ABSORPTION_SHADER,
	BASIC_SHADER,
	NORMAL_SHADER
};
enum VolumeType {
	HOMOGENEOUS = 0,
	HETEROGENEOUS = 1
};

// store the current shader selection
ShaderType currentShaderType = ABSORPTION_SHADER;
VolumeType currentVolumeType = HOMOGENEOUS;

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

VolumeMaterial::VolumeMaterial(double absorption_coefficient, glm::vec4 color, float noise_scale, int noise_detail, float step_length)
{
	this->color = color;
	this->absorption_coefficient = absorption_coefficient;
	this->noise_scale = noise_scale;
	this->noise_detail = noise_detail;
	this->step_length = step_length;
	this->basic_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/basic.fs");
	this->absorption_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/absorption.fs");
	this->normal_shader = Shader::Get("res/shaders/basic.vs", "res/shaders/normal.fs");
	this->shader = this->absorption_shader;
}

void VolumeMaterial::setUniforms(Camera* camera, glm::mat4 model, Mesh* mesh) {
	//Convert Camera position to Local Coordinates
	glm::mat4 inverseModel = glm::inverse(model);
	glm::vec4 temp = glm::vec4(camera->eye, 1.0);
	temp = inverseModel * temp;
	glm::vec3 local_camera_pos = glm::vec3(temp.x / temp.w, temp.y / temp.w, temp.z / temp.w);

	glm::vec3 boxMin = mesh->aabb_min;
	glm::vec3 boxMax = mesh->aabb_max;

	//glm::vec3 rayOrigin = local_camera_pos;
	//glm::vec3 rayDir = glm::normalize(boxMin - rayOrigin);

	////intersection function
	//glm::vec3 tMin = (boxMin - rayOrigin) / rayDir;
	//glm::vec3 tMax = (boxMax - rayOrigin) / rayDir;
	//glm::vec3 t1 = glm::min(tMin, tMax);
	//glm::vec3 t2 = glm::max(tMin, tMax);

	//float ta = glm::max(glm::max(t1.x, t1.y), t1.z);  // Starting intersection
	//float tb = glm::min(glm::min(t2.x, t2.y), t2.z);  // Ending intersection

	////upload node uniforms
	//this->shader->setUniform("u_ending_position", ta);
	//this->shader->setUniform("u_ending_position", tb);

	this->shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	this->shader->setUniform("u_camera_position", local_camera_pos);
	this->shader->setUniform("u_model", model);

	this->shader->setUniform("u_color", this->color);
	this->shader->setUniform("u_absorption_coefficient", this->absorption_coefficient);

	this->shader->setUniform("u_boxMin", mesh->aabb_min);
	this->shader->setUniform("u_boxMax", mesh->aabb_max);

	this->shader->setUniform("u_ambient_light", Application::instance->ambient_light);
	this->shader->setUniform("u_background_color", Application::instance->background_color);

	int volumeTypeInt = static_cast<int>(currentVolumeType);
	this->shader->setUniform("u_volume_type", volumeTypeInt);
	this->shader->setUniform("u_step_length", this->step_length);
	this->shader->setUniform("u_noise_scale", this->noise_scale);
	this->shader->setUniform("u_noise_detail", this->noise_detail);


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
	ImGui::ColorEdit3("Color", (float*)&this->color);
	ImGui::SliderFloat("Absorption Coefficient", &this->absorption_coefficient, 0.0f, 2.0f); // Absorption control
	// Dropdown menu for shader selection

	const char* shaderNames[] = { "Absorption Shader", "Basic Shader", "Normal Shader" };
	const char* volumeTypeNames[] = { "Homogeneous", "Heterogeneous" };
	int shaderIndex = static_cast<int>(currentShaderType);  // Convert enum to int for ImGui
	int volumeTypeIndex = static_cast<int>(currentVolumeType);  // Convert enum to int for ImGui

	if (ImGui::Combo("Shader Type", &shaderIndex, shaderNames, IM_ARRAYSIZE(shaderNames))) {
		currentShaderType = static_cast<ShaderType>(shaderIndex);  // Update enum based on selection
		setShader();
	}
	if (ImGui::Combo("Volume Type", &volumeTypeIndex, volumeTypeNames, IM_ARRAYSIZE(volumeTypeNames))) {
		currentVolumeType = static_cast<VolumeType>(volumeTypeIndex);  // Update enum based on selection
	}
	if (static_cast<int>(currentVolumeType) == 1) {
		ImGui::SliderFloat("Step Length", &this->step_length, 0.01f, 3.0f); // Absorption control
		//ImGui::SliderFloat("Noise Detail", &this->noise_detail, 0.0f, 5.0f); // Absorption control
		ImGui::SliderInt("Noise Detail", &this->noise_detail, 0, 5);
		ImGui::SliderFloat("Noise Scale", &this->noise_scale, 0.0f, 5.0f); // Absorption control
	}

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
	}
}
