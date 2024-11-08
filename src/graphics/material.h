#pragma once

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/matrix.hpp>

#include "../framework/camera.h"
#include "mesh.h"
#include "texture.h"
#include "shader.h"

class Material {
public:

	Shader* shader = NULL;
	Texture* texture = NULL;
	glm::vec4 color;

	virtual void setUniforms(Camera* camera, glm::mat4 model) = 0;
	virtual void render(Mesh* mesh, glm::mat4 model, Camera* camera) = 0;
	virtual void renderInMenu() = 0;
};

class FlatMaterial : public Material {
public:

	FlatMaterial(glm::vec4 color = glm::vec4(1.f));
	~FlatMaterial();

	void setUniforms(Camera* camera, glm::mat4 model);
	void render(Mesh* mesh, glm::mat4 model, Camera* camera);
	void renderInMenu();
};

class WireframeMaterial : public FlatMaterial {
public:

	WireframeMaterial();
	~WireframeMaterial();

	void render(Mesh* mesh, glm::mat4 model, Camera* camera);
};

class StandardMaterial : public Material {
public:

	bool first_pass = false;

	bool show_normals = false;
	Shader* base_shader = NULL;
	Shader* normal_shader = NULL;

	StandardMaterial(glm::vec4 color = glm::vec4(1.f));
	~StandardMaterial();

	void setUniforms(Camera* camera, glm::mat4 model);
	void render(Mesh* mesh, glm::mat4 model, Camera* camera);
	void renderInMenu();
};

class VolumeMaterial : public Material {
public:
	float absorption_coefficient;
	float noise_scale;
	int noise_detail;
	float step_length;
	glm::vec4 color;

	Shader* basic_shader = NULL;
	Shader* absorption_shader = NULL;
	Shader* normal_shader = NULL;

	VolumeMaterial(double absorption_coefficient = 1.0, glm::vec4 color = glm::vec4(0.f),
		float noise_scale = 1.558f, int noise_detail = 5.f, float step_length = 0.004f);

	void setUniforms(Camera* camera, glm::mat4 model) override {
		setUniforms(camera, model, nullptr);
	};
	void setUniforms(Camera* camera, glm::mat4 model, Mesh* mesh);
	void render(Mesh* mesh, glm::mat4 model, Camera* camera) override;
	void renderInMenu() override;
	void setShader();

};