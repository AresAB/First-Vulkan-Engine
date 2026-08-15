#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <stdlib.h>
#include <string.h>

typedef struct Vec3 {
	float x;
	float y;
	float z;
} Vec3;

typedef struct Vec2 {
	float x;
	float y;
} Vec2;

typedef struct Vertex {
	Vec3 position;
	Vec3 normal;
	Vec2 uv;
} Vertex;

typedef struct IndicedVertex {
	Vertex v;
	uint16_t i;
} IndicedVertex;

typedef struct Mesh {
	VkDeviceSize i_index;
	VkDeviceSize i_count;
} Mesh;

typedef struct Model {
	VmaAllocation v_buffer_allocation{ VK_NULL_HANDLE };
	VkBuffer v_buffer{ VK_NULL_HANDLE };
	VkDeviceSize v_size;
	Mesh* meshes;
	uint16_t mesh_count;
} Model;

void initialize_p_verts(IndicedVertex** p_verts, Vec3* normals, Vec2* uv, VkDeviceSize v_size, FILE* file, long int end_i) {
	char* str_read = (char*)malloc(1000 * sizeof(char));
	VkDeviceSize v_i = 0;
	VkDeviceSize vn_i = 0;
	VkDeviceSize vt_i = 0;
	while(ftell(file) != end_i) {
		char c_read = fgetc(file);
		if(c_read == 'v') {
			c_read = fgetc(file);
			if(c_read == 't') {
				fseek(file, 1, SEEK_CUR);
				float* texcoord = &(uv[vt_i].x);
				for(uint16_t j = 0; j < 2; j++) {
					uint16_t str_i = 0;
					c_read = fgetc(file);
					while(c_read != ' ' && c_read != '\n') {	
						str_read[str_i] = c_read;
						str_i++;
						c_read = fgetc(file);
					}
					str_read[str_i] = '\0';
					texcoord[j] = (float)atof(str_read);
				}
				if(c_read != '\n') fgets(str_read,1000,file);
				vt_i++;
			}
			else if(c_read == 'n') {
				fseek(file, 1, SEEK_CUR);
				float* normal = &(normals[vn_i].x);
				for(uint16_t j = 0; j < 3; j++) {
					uint16_t str_i = 0;
					c_read = fgetc(file);
					while(c_read != ' ' && c_read != '\n') {	
						str_read[str_i] = c_read;
						str_i++;
						c_read = fgetc(file);
					}
					str_read[str_i] = '\0';
					normal[j] = (float)atof(str_read);
				}
				vn_i++;
			}
			else {
				IndicedVertex* vert_struct = (IndicedVertex*)calloc(1, sizeof(IndicedVertex));
				vert_struct->i = 0;
				p_verts[v_i] = vert_struct;
				float* v_pos = &(vert_struct->v.position.x);
				for(uint16_t j = 0; j < 3; j++) {
					uint16_t str_i = 0;
					c_read = fgetc(file);
					while(c_read != ' ' && c_read != '\n') {	
						str_read[str_i] = c_read;
						str_i++;
						c_read = fgetc(file);
					}
					str_read[str_i] = '\0';
					v_pos[j] = (float)atof(str_read);
				}
				v_i++;
			}
		}
		else if(c_read != '\n') {
			fgets(str_read,1000,file);
		}
	}
	free(str_read);
}

void generate_mesh_minimal(Mesh* p_mesh, FILE* file, IndicedVertex** p_verts, uint16_t v_size, uint16_t** p_indices, long int* beg_indices_and_f_sizes) {
	long int f_size = beg_indices_and_f_sizes[1];
	uint16_t* indices = (uint16_t*)malloc(sizeof(uint16_t) * f_size * 3);
	*p_indices = indices;
	uint16_t indice_i = 0;
	char c_read;
	char* str_read = (char*)malloc(100 * sizeof(char));
	fseek(file, beg_indices_and_f_sizes[0], SEEK_SET);
	c_read = fgetc(file);
	printf("char: %c at %ld\n", c_read, beg_indices_and_f_sizes[0]);
	while(c_read == 'f') {
		fseek(file, 1, SEEK_CUR);
		for(uint16_t i = 0; i < 3; i++) {
			uint16_t str_i = 0;
			c_read = fgetc(file);
			while(c_read != ' ' && c_read != '\n') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			indices[indice_i*3+i] = atoi(str_read) - 1;
		}
		Vec3 a = p_verts[indices[3*indice_i]]->v.position;
		Vec3 ab = p_verts[indices[3*indice_i+1]]->v.position;
		ab.x -= a.x;
		ab.y -= a.y;
		ab.z -= a.z;
		Vec3 ac = p_verts[indices[3*indice_i+2]]->v.position;
		ac.x -= a.x;
		ac.y -= a.y;
		ac.z -= a.z;
		Vec3 normal = {
			ab.y * ac.z - ab.z * ac.y,
			ab.z * ac.x - ab.x * ac.z,
			ab.x * ac.y - ab.y * ac.x
		};
		float mag = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
		normal.x /= mag;
		normal.y /= mag;
		normal.z /= mag;
		p_verts[indices[3*indice_i]]->v.normal.x +=normal.x;
		p_verts[indices[3*indice_i]]->v.normal.y +=normal.y;
		p_verts[indices[3*indice_i]]->v.normal.z +=normal.z;
		p_verts[indices[3*indice_i]]->i++;
		p_verts[indices[3*indice_i+1]]->v.normal.x +=normal.x;
		p_verts[indices[3*indice_i+1]]->v.normal.y +=normal.y;
		p_verts[indices[3*indice_i+1]]->v.normal.z +=normal.z;
		p_verts[indices[3*indice_i+1]]->i++;
		p_verts[indices[3*indice_i+2]]->v.normal.x +=normal.x;
		p_verts[indices[3*indice_i+2]]->v.normal.y +=normal.y;
		p_verts[indices[3*indice_i+2]]->v.normal.z +=normal.z;
		p_verts[indices[3*indice_i+2]]->i++;
		indice_i++;
		c_read = fgetc(file);
	}
	free(str_read);
	Mesh new_m;
	new_m.i_count = indice_i * 3;
	*p_mesh = new_m;
}

/*
uint16_t generate_mesh_given_normals(Mesh** pp_mesh, IndicedVertex** pa_vert, Vec3* normals, uint16_t v_size, FILE* file, uint16_t* beg_indices_and_f_sizes) {
	uint16_t section_size = sizeof(beg_indices_and_f_sizes) / sizeof(uint16_t);
	uint16_t f_size = 0;
	for(uint16_t i = 1; i < section_size; i += 2) {
		f_size += beg_indices_and_f_sizes[i];
	}
	Mesh* new_m = (Mesh*)malloc(sizeof(Mesh));
	*pp_mesh = new_m;
	new_m->indices = (uint16_t*)malloc(sizeof(uint16_t) * f_size * 6);
	uint16_t new_v_size = v_size;
	uint16_t indice_i = 0;
	char c_read;
	char* str_read = (char*)malloc(1000 * sizeof(char));
	for(uint16_t i = 0; i < section_size; i += 2){
		fseek(file, beg_indices_and_f_sizes[i], SEEK_SET);
		c_read = fgetc(file);
		while(c_read == 'f') {
			fseek(file, 1, SEEK_CUR);
			for(uint16_t j = 0; j < 3; j++) {
				uint16_t str_i = 0;
				c_read = fgetc(file);
				while(c_read != '/') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t v_indice = atoi(str_read) - 1;
				str_i = 0;
				c_read = fgetc(file);
				c_read = fgetc(file);
				while(c_read != ' ' && c_read != '\n') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t vn_indice = atoi(str_read) - 1;
				uint16_t indice = 0;
				uint16_t s = pa_vert[v_indice][0].i;
				if(s == 0) {
					pa_vert[v_indice][0].v.normal = normals[vn_indice];
					pa_vert[v_indice][0].i++;
					indice = v_indice + 1;
				}
				for(uint16_t k = 0; k < s; k++) {
					if(pa_vert[v_indice][k].v.normal.x == normals[vn_indice].x && pa_vert[v_indice][k].v.normal.y == normals[vn_indice].y && pa_vert[v_indice][k].v.normal.z == normals[vn_indice].z) {
						if(k == 0) indice = v_indice + 1;
						else indice = pa_vert[v_indice][k].i + 1;
						break;
					}
				}
				if(indice == 0) {
					pa_vert[v_indice][0].i++;
					pa_vert[v_indice] = (IndicedVertex*)realloc((void*)(pa_vert[v_indice]), (s+1) * sizeof(IndicedVertex));
					pa_vert[v_indice][s] = pa_vert[v_indice][0];
					pa_vert[v_indice][s].v.normal = normals[vn_indice];
					pa_vert[v_indice][s].i = new_v_size;
					new_v_size++;
					indice = new_v_size;
				}
				new_m->indices[indice_i*3+j] = indice - 1;
			}
			if(c_read == ' ') {
			c_read = fgetc(file);
			if(c_read != '\n') {
				indice_i++;
				uint16_t str_i = 0;
				while(c_read != '/') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t v_indice = atoi(str_read) - 1;
				str_i = 0;
				c_read = fgetc(file);
				c_read = fgetc(file);
				while(c_read != '\n') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t vn_indice = atoi(str_read) - 1;
				uint16_t indice = 0;
				uint16_t s = pa_vert[v_indice][0].i;
				if(s == 0) {
					pa_vert[v_indice][0].v.normal = normals[vn_indice];
					pa_vert[v_indice][0].i++;
					indice = v_indice + 1;
				}
				for(uint16_t j = 0; j < s; j++) {
					if(pa_vert[v_indice][j].v.normal.x == normals[vn_indice].x && pa_vert[v_indice][j].v.normal.y == normals[vn_indice].y && pa_vert[v_indice][j].v.normal.z == normals[vn_indice].z) {
						if(j == 0) indice = v_indice + 1;
						else indice = pa_vert[v_indice][j].i + 1;
						break;
					}
				}
				if(indice == 0) {
					pa_vert[v_indice][0].i++;
					pa_vert[v_indice] = (IndicedVertex*)realloc((void*)(pa_vert[v_indice]), (s+1) * sizeof(IndicedVertex));
					pa_vert[v_indice][s] = pa_vert[v_indice][0];
					pa_vert[v_indice][s].v.normal = normals[vn_indice];
					pa_vert[v_indice][s].i = new_v_size;
					new_v_size++;
					indice = new_v_size;
				}
				new_m->indices[indice_i*3] = new_m->indices[(indice_i-1)*3];
				new_m->indices[indice_i*3 + 1] = new_m->indices[(indice_i-1)*3 + 2];
				new_m->indices[indice_i*3 + 2] = indice - 1;
			}
			}
			indice_i++;
			c_read = fgetc(file);
		}
	}
	free(str_read);
	new_m->indices = (uint16_t*)realloc(new_m->indices, sizeof(uint16_t) * indice_i * 3);
	new_m->num_indices = indice_i * 3;
	new_m->num_vertices = new_v_size - v_size;
	return new_v_size;
}

uint16_t generate_mesh_given_uv(Mesh** pp_mesh, IndicedVertex** pa_vert, Vec2* uv, uint16_t v_size, FILE* file, uint16_t* beg_indices_and_f_sizes) {
	uint16_t section_size = sizeof(beg_indices_and_f_sizes) / sizeof(uint16_t);
	uint16_t f_size = 0;
	for(uint16_t i = 1; i < section_size; i += 2) {
		f_size += beg_indices_and_f_sizes[i];
	}
	Mesh* new_m = (Mesh*)malloc(sizeof(Mesh));
	*pp_mesh = new_m;
	new_m->indices = (uint16_t*)malloc(sizeof(uint16_t) * f_size * 6);
	uint16_t new_v_size = v_size;
	uint16_t indice_i = 0;
	char c_read;
	char* str_read = (char*)malloc(1000 * sizeof(char));
	uint16_t* face = (uint16_t*)malloc(8 * sizeof(uint16_t));
	for(uint16_t i = 0; i < section_size; i += 2){
		fseek(file, beg_indices_and_f_sizes[i], SEEK_SET);
		c_read = fgetc(file);
		while(c_read == 'f') {
			fseek(file, 1, SEEK_CUR);
			for(uint16_t j = 0; j < 3; j++) {
				uint16_t str_i = 0;
				c_read = fgetc(file);
				while(c_read != '/') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t v_indice = atoi(str_read) - 1;
				face[j*2] = v_indice;
				str_i = 0;
				c_read = fgetc(file);
				while(c_read != ' ' && c_read != '\n') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t vt_indice = atoi(str_read) - 1;
				uint16_t indice = 0;
				uint16_t s = pa_vert[v_indice][0].i;
				if(s == 0) {
					pa_vert[v_indice][0].v.uv = uv[vt_indice];
					pa_vert[v_indice][0].i++;
					indice = v_indice + 1;
					face[j*2+1] = 0;
				}
				for(uint16_t k = 0; k < s; k++) {
					if(pa_vert[v_indice][k].v.uv.x == uv[vt_indice].x && pa_vert[v_indice][k].v.uv.y == uv[vt_indice].y) {
						if(k == 0) indice = v_indice + 1;
						else indice = pa_vert[v_indice][k].i + 1;
						face[j*2+1] = k;
						break;
					}
				}
				if(indice == 0) {
					pa_vert[v_indice][0].i++;
					pa_vert[v_indice] = (IndicedVertex*)realloc((void*)(pa_vert[v_indice]), (s+1) * sizeof(IndicedVertex));
					pa_vert[v_indice][s] = pa_vert[v_indice][0];
					pa_vert[v_indice][s].v.uv = uv[vt_indice];
					pa_vert[v_indice][s].i = new_v_size;
					new_v_size++;
					indice = new_v_size;
					face[j*2+1] = s;
				}
				new_m->indices[indice_i*3+j] = indice - 1;
			}
			Vec3 a = pa_vert[face[0]][face[1]].v.position;
			Vec3 ab = pa_vert[face[2]][face[3]].v.position;
			ab.x -= a.x;
			ab.y -= a.y;
			ab.z -= a.z;
			Vec3 ac = pa_vert[face[4]][face[5]].v.position;
			ac.x -= a.x;
			ac.y -= a.y;
			ac.z -= a.z;
			Vec3 normal = {
				ab.y * ac.z - ab.z * ac.y,
				ab.z * ac.x - ab.x * ac.z,
				ab.x * ac.y - ab.y * ac.x
			};
			float mag = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
			normal.x /= mag;
			normal.y /= mag;
			normal.z /= mag;
			pa_vert[face[0]][face[1]].v.normal.x +=normal.x;
			pa_vert[face[0]][face[1]].v.normal.y +=normal.y;
			pa_vert[face[0]][face[1]].v.normal.z +=normal.z;
			pa_vert[face[0]][face[1]].v.uv.x += 10;
			pa_vert[face[2]][face[3]].v.normal.x +=normal.x;
			pa_vert[face[2]][face[3]].v.normal.y +=normal.y;
			pa_vert[face[2]][face[3]].v.normal.z +=normal.z;
			pa_vert[face[2]][face[3]].v.uv.x += 10;
			pa_vert[face[4]][face[5]].v.normal.x +=normal.x;
			pa_vert[face[4]][face[5]].v.normal.y +=normal.y;
			pa_vert[face[4]][face[5]].v.normal.z +=normal.z;
			pa_vert[face[4]][face[5]].v.uv.x += 10;
			if(c_read == ' ') {
			c_read = fgetc(file);
			if(c_read != '\n') {
				indice_i++;
				uint16_t str_i = 0;
				while(c_read != '/') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t v_indice = atoi(str_read) - 1;
				face[6] = v_indice;
				str_i = 0;
				c_read = fgetc(file);
				while(c_read != '\n') {	
					str_read[str_i] = c_read;
					str_i++;
					c_read = fgetc(file);
				}
				str_read[str_i] = '\0';
				uint16_t vt_indice = atoi(str_read) - 1;
				uint16_t indice = 0;
				uint16_t s = pa_vert[v_indice][0].i;
				if(s == 0) {
					pa_vert[v_indice][0].v.uv = uv[vt_indice];
					pa_vert[v_indice][0].i++;
					indice = v_indice + 1;
					face[7] = 0;
				}
				for(uint16_t k = 0; k < s; k++) {
					if(pa_vert[v_indice][k].v.uv.x == uv[vt_indice].x && pa_vert[v_indice][k].v.uv.y == uv[vt_indice].y) {
						if(k == 0) indice = v_indice + 1;
						else indice = pa_vert[v_indice][k].i + 1;
						face[7] = k;
						break;
					}
				}
				if(indice == 0) {
					pa_vert[v_indice][0].i++;
					pa_vert[v_indice] = (IndicedVertex*)realloc((void*)(pa_vert[v_indice]), (s+1) * sizeof(IndicedVertex));
					pa_vert[v_indice][s] = pa_vert[v_indice][0];
					pa_vert[v_indice][s].v.uv = uv[vt_indice];
					pa_vert[v_indice][s].i = new_v_size;
					new_v_size++;
					indice = new_v_size;
					face[7] = s;
				}
				new_m->indices[indice_i*3] = new_m->indices[(indice_i-1)*3];
				new_m->indices[indice_i*3 + 1] = new_m->indices[(indice_i-1)*3 + 2];
				new_m->indices[indice_i*3 + 2] = indice - 1;
				Vec3 a = pa_vert[face[0]][face[1]].v.position;
				Vec3 ab = pa_vert[face[4]][face[5]].v.position;
				ab.x -= a.x;
				ab.y -= a.y;
				ab.z -= a.z;
				Vec3 ac = pa_vert[face[6]][face[7]].v.position;
				ac.x -= a.x;
				ac.y -= a.y;
				ac.z -= a.z;
				Vec3 normal = {
					ab.y * ac.z - ab.z * ac.y,
					ab.z * ac.x - ab.x * ac.z,
					ab.x * ac.y - ab.y * ac.x
				};
				float mag = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
				normal.x /= mag;
				normal.y /= mag;
				normal.z /= mag;
				pa_vert[face[0]][face[1]].v.normal.x +=normal.x;
				pa_vert[face[0]][face[1]].v.normal.y +=normal.y;
				pa_vert[face[0]][face[1]].v.normal.z +=normal.z;
				pa_vert[face[0]][face[1]].v.uv.x += 10;
				pa_vert[face[4]][face[5]].v.normal.x +=normal.x;
				pa_vert[face[4]][face[5]].v.normal.y +=normal.y;
				pa_vert[face[4]][face[5]].v.normal.z +=normal.z;
				pa_vert[face[4]][face[5]].v.uv.x += 10;
				pa_vert[face[6]][face[7]].v.normal.x +=normal.x;
				pa_vert[face[6]][face[7]].v.normal.y +=normal.y;
				pa_vert[face[6]][face[7]].v.normal.z +=normal.z;
				pa_vert[face[6]][face[7]].v.uv.x += 10;
			}
			}
			indice_i++;
			c_read = fgetc(file);
		}
	}
	free(str_read);
	free(face);
	new_m->indices = (uint16_t*)realloc(new_m->indices, sizeof(uint16_t) * indice_i * 3);
	new_m->num_indices = indice_i * 3;
	new_m->num_vertices = new_v_size - v_size;
	return new_v_size;
}
*/

uint16_t generate_mesh(Mesh* p_mesh, FILE* file, IndicedVertex** p_verts, Vec3* normals, Vec2* uv, uint16_t v_size, uint16_t** p_indices, long int* beg_indices_and_f_sizes) {
	uint16_t f_size = beg_indices_and_f_sizes[1];
	uint16_t* indices = (uint16_t*)malloc(sizeof(uint16_t) * f_size * 6);
	*p_indices = indices;
	uint16_t new_v_size = v_size;
	uint16_t indice_i = 0;
	char c_read;
	char* str_read = (char*)malloc(1000 * sizeof(char));
	fseek(file, beg_indices_and_f_sizes[1], SEEK_SET);
	c_read = fgetc(file);
	while(c_read == 'f') {
		fseek(file, 1, SEEK_CUR);
		for(uint16_t j = 0; j < 3; j++) {
			uint16_t str_i = 0;
			c_read = fgetc(file);
			while(c_read != '/') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t v_indice = atoi(str_read) - 1;
			str_i = 0;
			c_read = fgetc(file);
			while(c_read != '/') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t vt_indice = atoi(str_read) - 1;
			str_i = 0;
			c_read = fgetc(file);
			while(c_read != ' ' && c_read != '\n') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t vn_indice = atoi(str_read) - 1;
			uint16_t indice = 0;
			uint16_t s = p_verts[v_indice][0].i;
			if(s == 0) {
				p_verts[v_indice][0].v.normal = normals[vn_indice];
				p_verts[v_indice][0].v.uv = uv[vt_indice];
				p_verts[v_indice][0].i++;
				indice = v_indice + 1;
			}
			for(uint16_t k = 0; k < s; k++) {
				if(p_verts[v_indice][k].v.uv.x == uv[vt_indice].x && p_verts[v_indice][k].v.uv.y == uv[vt_indice].y && p_verts[v_indice][k].v.normal.x == normals[vn_indice].x && p_verts[v_indice][k].v.normal.y == normals[vn_indice].y && p_verts[v_indice][k].v.normal.z == normals[vn_indice].z) {
					if(k == 0) indice = v_indice + 1;
					else indice = p_verts[v_indice][k].i + 1;
					break;
				}
			}
			if(indice == 0) {
				p_verts[v_indice][0].i++;
				p_verts[v_indice] = (IndicedVertex*)realloc((void*)(p_verts[v_indice]), (s+1) * sizeof(IndicedVertex));
				p_verts[v_indice][s] = p_verts[v_indice][0];
				p_verts[v_indice][s].v.uv = uv[vt_indice];
				p_verts[v_indice][s].v.normal = normals[vn_indice];
				p_verts[v_indice][s].i = new_v_size;
				new_v_size++;
				indice = new_v_size;
			}
			indices[indice_i*3+j] = indice - 1;
		}
		if(c_read == ' ') {
		c_read = fgetc(file);
		if(c_read != '\n') {
			indice_i++;
			uint16_t str_i = 0;
			while(c_read != '/') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t v_indice = atoi(str_read) - 1;
			str_i = 0;
			c_read = fgetc(file);
			while(c_read != '/') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t vt_indice = atoi(str_read) - 1;
			str_i = 0;
			c_read = fgetc(file);
			while(c_read != '\n') {	
				str_read[str_i] = c_read;
				str_i++;
				c_read = fgetc(file);
			}
			str_read[str_i] = '\0';
			uint16_t vn_indice = atoi(str_read) - 1;
			uint16_t indice = 0;
			uint16_t s = p_verts[v_indice][0].i;
			if(s == 0) {
				p_verts[v_indice][0].v.normal = normals[vn_indice];
				p_verts[v_indice][0].v.uv = uv[vt_indice];
				p_verts[v_indice][0].i++;
				indice = v_indice + 1;
			}
			for(uint16_t j = 0; j < s; j++) {
				if(p_verts[v_indice][j].v.uv.x == uv[vt_indice].x && p_verts[v_indice][j].v.uv.y == uv[vt_indice].y && p_verts[v_indice][j].v.normal.x == normals[vn_indice].x && p_verts[v_indice][j].v.normal.y == normals[vn_indice].y && p_verts[v_indice][j].v.normal.z == normals[vn_indice].z) {
					if(j == 0) indice = v_indice + 1;
					else indice = p_verts[v_indice][j].i + 1;
					break;
				}
			}
			if(indice == 0) {
				p_verts[v_indice][0].i++;
				p_verts[v_indice] = (IndicedVertex*)realloc((void*)(p_verts[v_indice]), (s+1) * sizeof(IndicedVertex));
				p_verts[v_indice][s] = p_verts[v_indice][0];
				p_verts[v_indice][s].v.uv = uv[vt_indice];
				p_verts[v_indice][s].v.normal = normals[vn_indice];
				p_verts[v_indice][s].i = new_v_size;
				new_v_size++;
				indice = new_v_size;
			}
			indices[indice_i*3] = indices[(indice_i-1)*3];
			indices[indice_i*3 + 1] = indices[(indice_i-1)*3 + 2];
			indices[indice_i*3 + 2] = indice - 1;
		}
		}
		indice_i++;
		c_read = fgetc(file);
	}
	free(str_read);
	indices = (uint16_t*)realloc(indices, sizeof(uint16_t) * indice_i * 3);
	Mesh new_m;
	new_m.i_count = indice_i * 3;
	*p_mesh = new_m;
	return new_v_size;
}

void load_model(VmaAllocator allocator, Model* new_m, const char* filename) {
	FILE* file = fopen(filename, "r");
	
	fseek(file, 0, SEEK_END);
	long int end_i = ftell(file);
	rewind(file);

	VkDeviceSize v_size = 0;
	VkDeviceSize vn_size = 0;
	VkDeviceSize vt_size = 0;
	uint16_t group_size = 0;
	uint16_t group_beg_indice_i = 0;
	long int** p_group_beg_indices_and_f_sizes = (long int**)malloc(10 * sizeof(long int*));
	char str_read[1000];
	char c_read;
	while(ftell(file) != end_i) {
		c_read = fgetc(file);
		if(c_read == 'v') {
			c_read = fgetc(file);
			if(c_read == 'n') vn_size++;
			else if(c_read == 't') vt_size++;
			else v_size++;
		}
		else if(c_read == 'f') {
			if(group_size == 0) {
				p_group_beg_indices_and_f_sizes[group_size] = (long int*)malloc(2*sizeof(long int));
				group_size = 1;
			}
			if(group_beg_indice_i > 0) p_group_beg_indices_and_f_sizes[group_size-1] = (long int*)realloc(p_group_beg_indices_and_f_sizes[group_size-1], (2*group_beg_indice_i+2) * sizeof(long int));
			p_group_beg_indices_and_f_sizes[group_size-1][2*group_beg_indice_i] = ftell(file)-1L;
			long int f_size = 1;
			while(c_read == 'f') {
				f_size++;
				if(ftell(file) == end_i) break;
				fgets(str_read,1000,file);
				c_read = fgetc(file);
			}
			fseek(file,-2,SEEK_CUR);
			p_group_beg_indices_and_f_sizes[group_size-1][2*group_beg_indice_i+1] = f_size;
			group_beg_indice_i++;
		}
		else if(c_read == 'm') {
			fseek(file, -1, SEEK_CUR);
			fgets(str_read,8,file);
			if(strcmp(str_read, "mtllib ") == 0) {
				// load material via filename
				// immplement later
			}
			else {
				fseek(file, -7, SEEK_CUR);
			}
		}
		else if(c_read == 'u') {
			fseek(file, -1, SEEK_CUR);
			fgets(str_read,8,file);
			if(strcmp(str_read, "usemtl ") == 0) {
				group_size++;
				if((group_size-1) % 10 == 0 && group_size != 1) p_group_beg_indices_and_f_sizes = (long int**)realloc(p_group_beg_indices_and_f_sizes, (group_size + 9) * sizeof(long int*));
				group_beg_indice_i = 0;
				p_group_beg_indices_and_f_sizes[group_size-1] = (long int*)malloc(2*sizeof(long int));
			}
			else {
				fseek(file, -7, SEEK_CUR);
			}
		}
		else if(c_read == 'p') {
			printf("ERROR: obj model uses points, which isn't supported\n");
		}
		else if(c_read == 'l') {
			printf("ERROR: obj model uses a polyline, which isn't supported\n");
		}
		if(c_read != '\n') fgets(str_read,1000,file);
	}
	rewind(file);

	IndicedVertex** p_verts = (IndicedVertex**)malloc(v_size * sizeof(IndicedVertex*));
	Vec3* normals = (Vec3*)malloc(vn_size * sizeof(Vec3));
	Vec2* uv = (Vec2*)malloc(vt_size * sizeof(Vec2));
	initialize_p_verts(p_verts, normals, uv, v_size, file, end_i);
	
	new_m->meshes = (Mesh*)malloc(group_size * sizeof(Mesh));
	new_m->mesh_count = group_size;

	VkDeviceSize old_v_size = v_size;
	uint16_t** p_indices = (uint16_t**)malloc(group_size * sizeof(uint16_t*));
	//if(vn_size == 0 && vt_size == 0) {
		for(uint16_t i = 0; i < group_size; i++) {
			generate_mesh_minimal(new_m->meshes+i, file, p_verts, v_size, p_indices+i, p_group_beg_indices_and_f_sizes[i]);
			free(p_group_beg_indices_and_f_sizes[i]);
		}
	//}
	/*
	else if(vt_size == 0) {
		for(uint16_t i = 0; i < group_size; i++) {
			v_size = generate_mesh_given_normals(new_m->pa_meshes+i, pa_vert, normals, v_size, new_m->VBO, file, pa_group_beg_indices_and_f_sizes[i]);
			free(pa_group_beg_indices_and_f_sizes[i]);
		}
	}
	else if(vn_size == 0) {
		for(uint16_t i = 0; i < group_size; i++) {
			v_size = generate_mesh_given_uv(new_m->pa_meshes+i, pa_vert, uv, v_size, new_m->VBO, file, pa_group_beg_indices_and_f_sizes[i]);
			free(pa_group_beg_indices_and_f_sizes[i]);
		}
	}
	else {
		for(uint16_t i = 0; i < group_size; i++) {
			v_size = generate_mesh(new_m->meshes+i, file, p_verts, normals, uv, v_size, p_indices+i, p_group_beg_indices_and_f_sizes[i]);
			free(p_group_beg_indices_and_f_sizes[i]);
		}
	}
	*/
	fclose(file);
	free(p_group_beg_indices_and_f_sizes);
	new_m->v_size = v_size * sizeof(Vertex);
	new_m->meshes[0].i_index = new_m->v_size;
	for(uint16_t i = 1; i < new_m->mesh_count; i++) {
		new_m->meshes[i].i_index = new_m->meshes[i-1].i_index + new_m->meshes[i-1].i_count * sizeof(uint16_t);
	}
	
	Vertex* vertices = (Vertex*)malloc(new_m->v_size);
	//if(vt_size == 0 && vn_size == 0){
		for(uint16_t i = 0; i < v_size; i++) {
			// since the index of p_verts[i][0] is = to i,
			// p_verts[i][0].i is instead used to store sum
			// of every instance of p_verts[i][*] in indices
			// used to calculate mean normal approximation
			vertices[i] = p_verts[i][0].v;
			vertices[i].position.y *= -1.0f;
			uint16_t sum = p_verts[i][0].i;
			vertices[i].normal.x /= sum;
			vertices[i].normal.y /= sum;
			vertices[i].normal.z /= sum;
			vertices[i].uv.y = 1.0f - vertices[i].uv.y;
			free(p_verts[i]);
		}
	//}
	/*
	else if(vn_size == 0) {
		for(uint16_t i = 0; i < old_v_size; i++) {
			vertices[i] = p_verts[i][0].v;
			// sum is encoded in uv.x as
			// uv.x + sum * 10
			uint16_t sum = floor(vertices[i].uv.x * 0.1f);
			
			vertices[i].uv.x -= 10 * sum;
			vertices[i].normal.x /= sum;
			vertices[i].normal.y /= sum;
			vertices[i].normal.z /= sum;
			for(uint16_t j = 1; j < p_verts[i][0].i; j++) {
				vertices[p_verts[i][j].i] = p_verts[i][j].v;
				sum = floor(vertices[p_verts[i][j].i].uv.x * 0.1f);
				vertices[p_verts[i][j].i].uv.x -= 10 * sum;
				vertices[p_verts[i][j].i].normal.x /= sum;
				vertices[p_verts[i][j].i].normal.y /= sum;
				vertices[p_verts[i][j].i].normal.z /= sum;
			}
		}
	}
	else {
		for(uint16_t i = 0; i < old_v_size; i++) {
			// since the index of p_verts[i][0] is = to i,
			// p_verts[i][0].i is instead used to store len
			vertices[i] = p_verts[i][0].v;
			for(uint16_t j = 1; j < p_verts[i][0].i; j++) {
				vertices[p_verts[i][j].i] = p_verts[i][j].v;
			}
			free(p_verts[i]);
		}
	}
	*/
	free(p_verts);

	printf("%ld, %ld\n", new_m->meshes[group_size-1].i_index, new_m->meshes[group_size-1].i_count * sizeof(uint16_t));
	VkBufferCreateInfo bufferCI {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = new_m->meshes[group_size-1].i_index + new_m->meshes[group_size-1].i_count * sizeof(uint16_t),
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
	};

	// First 2 flags tells VMA to create memory on the GPU (VRAM)
	// that is accessable to host. 3rd flags allows us to directly
	// copy data into VRAM.
	VmaAllocationCreateInfo v_buffer_allocCI {
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	VmaAllocationInfo v_buffer_alloc_info;
	VkResult result = vmaCreateBuffer(allocator, &bufferCI, &v_buffer_allocCI, &new_m->v_buffer, &new_m->v_buffer_allocation, &v_buffer_alloc_info);
	if(result != VK_SUCCESS) {
		printf("ERROR: VMA buffer for model ");printf(filename);printf(" failed to load (%d) at line %d\n", result, __LINE__);
		free(vertices);
		for(uint16_t i = 0; i < group_size; i++) {
			free(p_indices[i]);
		}
		free(p_indices);
		return;
	}

	memcpy(v_buffer_alloc_info.pMappedData, vertices, new_m->v_size);
	free(vertices);
	free(normals);
	free(uv);
	for(uint16_t i = 0; i < group_size; i++) {
		memcpy(((char*)v_buffer_alloc_info.pMappedData) + new_m->meshes[i].i_index, p_indices[i], new_m->meshes[i].i_count * sizeof(uint16_t));
		free(p_indices[i]);
	}
	free(p_indices);
}
