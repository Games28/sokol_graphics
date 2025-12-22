#include "sokol_engine.h"
#include "shd.glsl.h"

#include "math/v3d.h"
#include "math/mat4.h"

#include "mesh.h"

//for time
#include <ctime>


#include "texture.h"

//y p => x y z
//0 0 => 0 0 1
static vf3d polar3D(float yaw, float pitch) {
	return {
		std::sin(yaw)*std::cos(pitch),
		std::sin(pitch),
		std::cos(yaw)*std::cos(pitch)
	};
}

float randFloat(float b=1, float a=0) {
	static const float rand_max=RAND_MAX;
	float t=std::rand()/rand_max;
	return a+t*(b-a);
}

//https://stackoverflow.com/a/6178290
float randNormal() {
	float rho=std::sqrt(-2*std::log(randFloat()));
	float theta=2*Pi*randFloat();
	return rho*std::cos(theta);
}

//https://math.stackexchange.com/a/1585996
vf3d randDir() {
	return vf3d(
		randNormal(),
		randNormal(),
		randNormal()
	).norm();
}

vf3d rayIntersectPlane(const vf3d& orig, const vf3d& dir, const vf3d& ctr, const vf3d& norm) {
	float t=norm.dot(ctr-orig)/norm.dot(dir);
	return orig+t*dir;
}

//https://www.rapidtables.com/convert/color/hsv-to-rgb.html
static sg_color hsv2rgb(int h, float s, float v) {
	float c=v*s;
	float x=c*(1-std::abs(1-std::fmod(h/60.f, 2)));
	float m=v-c;
	float r=0, g=0, b=0;
	switch(h/60) {
		case 0: r=c, g=x, b=0; break;
		case 1: r=x, g=c, b=0; break;
		case 2: r=0, g=c, b=x; break;
		case 3: r=0, g=x, b=c; break;
		case 4: r=x, g=0, b=c; break;
		case 5: r=c, g=0, b=x; break;
	}
	return {m+r, m+g, m+b, 1};
}

static sg_color randomPastel() {
	//hue=pure color
	int h=std::rand()%360;
	//saturation=intensity
	float s=randFloat(.1f, .4f);
	//value=brightness
	float v=randFloat(.75f, 1);
	return hsv2rgb(h, s, v);
}

struct Object {
	Mesh mesh;
	sg_view tex{SG_INVALID_ID};
	bool draggable=false;
};


struct Light
{
	vf3d pos;
	sg_color col;
};

struct Demo : SokolEngine {
	sg_bindings bindings;
	sg_pipeline pipeline;

	//2d texture test
	struct {
		sg_pipeline pip{};
		sg_bindings bind{};
	} texview;

	//cam info
	vf3d cam_pos{0, 2, 2};
	vf3d cam_dir;
	float cam_yaw=-Pi;
	float cam_pitch=-Pi/4;

	//scene info
	std::list<Light> lights;
	Light* red_light, * green_light, * blue_light;

	bool renderquad = false;

	const std::vector<std::string> Structurefilenames{
			"assets/models/desert.txt",
			"assets/models/sandspeeder.txt",
			"assets/models/tathouse1.txt",
			"assets/models/tathouse2.txt",
	};

	bool fps_controls=false;

	//player kinematics
	float gravity = -9.8f, player_height = 0.25f, player_rad = 0.1f, player_airtime = 0;
	bool player_on_ground = false;
	vf3d player_pos, player_vel;

	sg_sampler sampler;
	
	sg_view tex_blank;
	sg_view tex_uv;
	sg_view tex_checker;

	std::vector<Object> objects;

	Object* billboard;

	mat4 view_proj;

	vf3d mouse_dir;
	vf3d prev_mouse_dir;

	//store object handle and grab plane
	Object* grab_obj=nullptr;
	vf3d grab_ctr, grab_norm;

	float color_timer=0;
	const float color_period=5;
	sg_color prev_col, next_col, curr_col;

#pragma region SETUP HELPERS
	
	sg_view getTexture(const std::string& filename)
	{
		
		
			sg_view tex_checker;
			sg_view tex_blank = makeBlankTexture();
			sg_view tex_uv = makeUVTexture(1024, 1024);
			if (filename == " ")
			{
				tex_checker = tex_blank;
			}
			else
			{
				makeTextureFromFile(tex_checker, filename);
			}

			return tex_checker;
		
		
	}

	
	
	void setupSampler() {
		sg_sampler_desc sampler_desc{};
		sampler=sg_make_sampler(sampler_desc);
		bindings.samplers[SMP_u_smp]=sampler;
	}


	void setupStructures()
	{
		Mesh m;
		auto status = Mesh::loadFromOBJ(m, Structurefilenames[0]);
		if (!status.valid) m = Mesh::makeCube();
		m.scale = { 8, .5f, 8 };
		m.translation = { 0, -2, 0 };
		m.updateMatrixes();
		

		objects.push_back({ m, getTexture("assets/img/sandtexture.png")});
	}

	void setupPlatform() {
		Mesh m;
		m = Mesh::makeCube();
		m.scale = { 8, .5f, 8 };
		m.translation = { 0, -2, 0 };
		m.updateMatrixes();

		objects.push_back({ m, getTexture(" ")});
	}

	void setupBillboard() {
		Mesh m;
		m.verts={
			{{-.5f, .5f, 0}, {0, 0, 1}, {0, 0}},//tl
			{{.5f, .5f, 0}, {0, 0, 1}, {1, 0}},//tr
			{{-.5f, -.5f, 0}, {0, 0, 1}, {0, 1}},//bl
			{{.5f, -.5f, 0}, {0, 0, 1}, {1, 1}}//br
		};
		m.tris={
			{0, 2, 1},
			{1, 2, 3}
		};
		m.updateVertexBuffer();
		m.updateIndexBuffer();
		m.updateMatrixes();

		objects.push_back({ m, getTexture("assets/img/tree2x100.png"),true});
		billboard=&objects.back();
	}

	void setupLights()
	{
		//red
		lights.push_back({ {-1,3,1},{1,0,0,1} });
		red_light = &lights.back();

		//green
		lights.push_back({ {-1,3,1},{0,1,0,1} });
		green_light = &lights.back();

		//blue
		lights.push_back({ {-1,3,1},{0,0,1,1} });
		blue_light = &lights.back();

	}

	void setupTexview()
	{
		//2d tristrip pipeline
		sg_pipeline_desc pip_desc{};
		pip_desc.layout.attrs[ATTR_texview_v_pos].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.layout.attrs[ATTR_texview_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		pip_desc.shader = sg_make_shader(texview_shader_desc(sg_query_backend()));
		pip_desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
		texview.pip = sg_make_pipeline(pip_desc);

		//quad vertex buffer: xyuv
		float vertexes[4][2][2]{
			{{-1, -1}, {0, 0}},//tl
			{{1, -1}, {1, 0}},//tr
			{{-1, 1}, {0, 1}},//bl
			{{1, 1}, {1, 1}}//br
		};
		sg_buffer_desc vbuf_desc{};
		vbuf_desc.data.ptr = vertexes;
		vbuf_desc.data.size = sizeof(vertexes);
		texview.bind.vertex_buffers[0] = sg_make_buffer(vbuf_desc);

		//sampler
		texview.bind.samplers[SMP_texview_smp] = sampler;
		//sg_pipeline_desc pip_desc{};
		//pip_desc.layout.attrs[ATTR_texview_v_pos].format = SG_VERTEXFORMAT_FLOAT2;
		//pip_desc.layout.attrs[ATTR_texview_v_uv].format = SG_VERTEXFORMAT_FLOAT2;
		//pip_desc.shader = sg_make_shader(texview_shader_desc(sg_query_backend()));
		//pip_desc.index_type = SG_INDEXTYPE_UINT32;
		//texview.pip = sg_make_pipeline(pip_desc);
		////bindings: just a quad
	    ////vertex buffer: xyuv
		//float vertexes[4][2][2]{
		//		{{-1, -1}, {0, 0}},//tl
		//		{{1, -1}, {1, 0}},//tr
		//		{{-1, 1}, {0, 1}},//bl
		//		{{1, 1}, {1, 1}}//br
		//};
		//sg_buffer_desc vbuf_desc{};
		//vbuf_desc.data.ptr = vertexes;
		//vbuf_desc.size = sizeof(vertexes);
		//texview.bind.vertex_buffers[0] = sg_make_buffer(vbuf_desc);
		//
		////index buffer
		//std::uint32_t indexes[2][3]{
		//		{0, 2, 1}, {1, 2, 3}
		//};
		//sg_buffer_desc ibuf_desc{};
		//ibuf_desc.usage.index_buffer = true;
		//ibuf_desc.data.ptr = indexes;
		//ibuf_desc.size = sizeof(indexes);
		//texview.bind.index_buffer = sg_make_buffer(ibuf_desc);
		//
		////sampler
		//texview.bind.samplers[SMP_texview_smp] = sampler;
	}
	

	void setupPipeline() {
		sg_pipeline_desc pipeline_desc{};
		pipeline_desc.shader=sg_make_shader(shd_shader_desc(sg_query_backend()));
		pipeline_desc.layout.attrs[ATTR_shd_pos].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_norm].format=SG_VERTEXFORMAT_FLOAT3;
		pipeline_desc.layout.attrs[ATTR_shd_uv].format=SG_VERTEXFORMAT_SHORT2N;
		pipeline_desc.index_type=SG_INDEXTYPE_UINT32;//use the index buffer
		pipeline_desc.cull_mode=SG_CULLMODE_FRONT;

		pipeline_desc.depth.write_enabled=true;//use depth buffer
		pipeline_desc.depth.compare=SG_COMPAREFUNC_LESS_EQUAL;
		pipeline=sg_make_pipeline(pipeline_desc);
	}
#pragma endregion

	void userCreate() override {
		app_title="Solids of Revolution & Billboarding Demo";

		std::srand(std::time(0));

		
		setupSampler();

		setupStructures();
		//setupPlatform();

		setupBillboard();

		setupLights();

		setupTexview();

		setupPipeline();

		//init rand cols
		prev_col=randomPastel();
		next_col=randomPastel();
	}

#pragma region UPDATE HELPERS



	
	void updateMatrixes() {
		//camera transformation matrix
		mat4 look_at=mat4::makeLookAt(cam_pos, cam_pos+cam_dir, {0, 1, 0});
		mat4 view=mat4::inverse(look_at);

		//perspective
		mat4 proj=mat4::makePerspective(90.f, sapp_widthf()/sapp_heightf(), .001f, 1000);

		//premultiply transform
		view_proj=mat4::mul(proj, view);
	}

	void updateMouseRay() {
		prev_mouse_dir=mouse_dir;

		//unprojection matrix
		mat4 inv_view_proj=mat4::inverse(view_proj);

		//mouse coords from clip -> world
		float ndc_x=2*mouse_x/sapp_widthf()-1;
		float ndc_y=1-2*mouse_y/sapp_heightf();
		vf3d clip(ndc_x, ndc_y, 1);
		float w=1;
		vf3d world=matMulVec(inv_view_proj, clip, w);
		world/=w;

		//normalize direction
		mouse_dir=(world-cam_pos).norm();
	}

	void handleCameraLooking(float dt) {
		//cant look while grabbing.
		if(grab_obj) return;

		//lock mouse position
		if(getKey(SAPP_KEYCODE_F).pressed) {
			fps_controls^=true;
			sapp_lock_mouse(fps_controls);
		}

		if(fps_controls) {
			//move with mouse
			const float sensitivity=0.5f*dt;

			//left/right
			cam_yaw-=sensitivity*mouse_dx;

			//up/down (y backwards
			cam_pitch-=sensitivity*mouse_dy;
		} else {
			//move with keys

			//left/right
			if(getKey(SAPP_KEYCODE_LEFT).held) cam_yaw+=dt;
			if(getKey(SAPP_KEYCODE_RIGHT).held) cam_yaw-=dt;

			//up/down
			if(getKey(SAPP_KEYCODE_UP).held) cam_pitch+=dt;
			if(getKey(SAPP_KEYCODE_DOWN).held) cam_pitch-=dt;
		}

		//clamp camera pitch
		if(cam_pitch>Pi/2) cam_pitch=Pi/2-.001f;
		if(cam_pitch<-Pi/2) cam_pitch=.001f-Pi/2;
	}

	void handleCameraMovement(float dt) {
		//cant move while grabbing.
		if(grab_obj) return;

		//move up, down
		if(getKey(SAPP_KEYCODE_SPACE).held) cam_pos.y+=4.f*dt;
		if(getKey(SAPP_KEYCODE_LEFT_SHIFT).held) cam_pos.y-=4.f*dt;

		//move forward, backward
		vf3d fb_dir(std::sin(cam_yaw), 0, std::cos(cam_yaw));
		if(getKey(SAPP_KEYCODE_W).held) cam_pos+=5.f*dt*fb_dir;
		if(getKey(SAPP_KEYCODE_S).held) cam_pos-=3.f*dt*fb_dir;

		//move left, right
		vf3d lr_dir(fb_dir.z, 0, -fb_dir.x);
		if(getKey(SAPP_KEYCODE_A).held) cam_pos+=4.f*dt*lr_dir;
		if(getKey(SAPP_KEYCODE_D).held) cam_pos-=4.f*dt*lr_dir;
	}

	void handleGrabActionBegin() {
		handleGrabActionEnd();

		//find closest mesh
		float record=-1;
		Object* close_obj=nullptr;
		for(auto& o:objects) {
			//is intersection valid?
			float dist=o.mesh.intersectRay(cam_pos, mouse_dir);
			if(dist<0) continue;

			//"sort" while iterating
			if(record<0||dist<record) {
				record=dist;
				close_obj=&o;
			}
		}
		if(!close_obj) return;

		//this way we cant grab behind things.
		if(!close_obj->draggable) return;

		grab_obj=close_obj;

		//place plane at intersection point
		//perp to cam view.
		grab_ctr=cam_pos+record*mouse_dir;
		grab_norm=cam_dir;
	}

	void handleGrabActionUpdate() {
		if(!grab_obj) return;

		//project delta onto grab plane
		vf3d prev_pt=rayIntersectPlane(cam_pos, prev_mouse_dir, grab_ctr, grab_norm);
		vf3d curr_pt=rayIntersectPlane(cam_pos, mouse_dir, grab_ctr, grab_norm);
		//move by delta.
		grab_obj->mesh.translation+=curr_pt-prev_pt;
		grab_obj->mesh.updateMatrixes();
	}

	void handleGrabActionEnd() {
		grab_obj=nullptr;
	}

	void handleUserInput(float dt) {
		handleCameraLooking(dt);

		//polar to cartesian
		cam_dir=polar3D(cam_yaw, cam_pitch);

		handleCameraMovement(dt);

		//grab & drag with left mouse button
		{
			const auto grab_action=getMouse(SAPP_MOUSEBUTTON_LEFT);
			if(grab_action.pressed) handleGrabActionBegin();
			if(grab_action.held) handleGrabActionUpdate();
			if(grab_action.released) handleGrabActionEnd();
		}

		if (getKey(SAPP_KEYCODE_R).held) red_light->pos = cam_pos;
		if (getKey(SAPP_KEYCODE_G).held) green_light->pos = cam_pos;
		if (getKey(SAPP_KEYCODE_B).held) blue_light->pos = cam_pos;


		if(getKey(SAPP_KEYCODE_F11).pressed) {
			sapp_toggle_fullscreen();
		}

		//exit on escape
		if(getKey(SAPP_KEYCODE_ESCAPE).pressed) sapp_request_quit();

		if (getKey(SAPP_KEYCODE_C).pressed) renderquad ^= true;
	}

	//make billboard always point at camera.
	void updateBillboard() {

		//move with player 
		vf3d eye_pos=billboard->mesh.translation;
		vf3d target=cam_pos;

		vf3d y_axis(0, 1, 0);
		vf3d z_axis=(target-eye_pos).norm();
		vf3d x_axis=y_axis.cross(z_axis).norm();
		y_axis=z_axis.cross(x_axis);

		//slightly different than makeLookAt.
		mat4 m;
		m(0, 0)=x_axis.x, m(0, 1)=y_axis.x, m(0, 2)=z_axis.x, m(0, 3)=eye_pos.x;
		m(1, 0)=x_axis.y, m(1, 1)=y_axis.y, m(1, 2)=z_axis.y, m(1, 3)=eye_pos.y;
		m(2, 0)=x_axis.z, m(2, 1)=y_axis.z, m(2, 2)=z_axis.z, m(2, 3)=eye_pos.z;
		m(3, 3)=1;

		billboard->mesh.model=m;
		billboard->mesh.inv_model=mat4::inverse(m);
	}

	void updateColor(float dt) {
		if(color_timer>color_period) {
			color_timer-=color_period;

			prev_col=next_col;
			next_col=randomPastel();
		}

		//lerp between prev & next
		float t=color_timer/color_period;
		curr_col=prev_col+t*(next_col-prev_col);

		color_timer+=dt;
	}
#pragma endregion

	void userUpdate(float dt) {
		updateMatrixes();

		updateMouseRay();

		handleUserInput(dt);

		

	 	updateBillboard();

		updateColor(dt);
	}

#pragma region RENDER HELPERS

	
	void renderQuad()
	{
		//const int tile_size = 200;
		//
		//
		//	texview.bind.views[VIEW_texview_tex] = getTexture("assets/img/tree2x100.png"); //shadow_map.faces[i].tex_color_view;
		//	sg_apply_bindings(texview.bind);
		//
		//	sg_apply_viewport(0, 0, tile_size, tile_size, true);
		//
		//	//4 verts = 1quad
		//	sg_draw(0, 4, 1);
		//

		const int tile_size = 200;
		const int idx[6][2]{
			{0, 0}, {1, 0}, {2, 0},
			{0, 1}, {1, 1}, {2, 1}
		};
		for (int i = 0; i < 1; i++) {
			sg_apply_pipeline(texview.pip);

			texview.bind.views[VIEW_texview_tex] = getTexture("assets/img/tree2x100.png"); //shadow_map.faces[i].tex_color_view;
			sg_apply_bindings(texview.bind);

			int x = tile_size * idx[i][0];
			int y = tile_size * idx[i][1];
			sg_apply_viewport(x, y, tile_size, tile_size, true);

			//4 verts = 1quad
			sg_draw(0, 4, 1);
		}
	}

	void renderObject(const Object& o) {
		//update bindings
		bindings.vertex_buffers[0]=o.mesh.vbuf;
		bindings.index_buffer=o.mesh.ibuf;
		bindings.views[VIEW_u_tex]=o.tex;
		sg_apply_bindings(bindings);

		//send vertex uniforms
		vs_params_t vs_params{};
		std::memcpy(vs_params.u_model, o.mesh.model.m, sizeof(vs_params.u_model));
		std::memcpy(vs_params.u_proj_view, view_proj.m, sizeof(vs_params.u_proj_view));
		sg_apply_uniforms(UB_vs_params, SG_RANGE(vs_params));

		sg_draw(0, 3*o.mesh.tris.size(), 1);
	}
#pragma endregion

	void userRender() {
		//still not sure what a pass is...
		sg_pass pass{};
		pass.action.colors[0].load_action=SG_LOADACTION_CLEAR;
		pass.action.colors[0].clear_value=curr_col;
		pass.swapchain=sglue_swapchain();
		sg_begin_pass(pass);

		sg_apply_pipeline(pipeline);

		fs_params_t fs_params{};
		{
			fs_params.u_num_lights = lights.size();
			int idx = 0;
			for (const auto& l : lights)
			{
				fs_params.u_light_pos[idx][0] = l.pos.x;
				fs_params.u_light_pos[idx][1] = l.pos.y;
				fs_params.u_light_pos[idx][2] = l.pos.z;
				fs_params.u_light_col[idx][0] = l.col.r;
				fs_params.u_light_col[idx][1] = l.col.g;
				fs_params.u_light_col[idx][2] = l.col.b;
				idx++;
			}
		}

		fs_params.u_view_pos[0] = cam_pos.x;
		fs_params.u_view_pos[1] = cam_pos.y;
		fs_params.u_view_pos[2] = cam_pos.z;
		sg_apply_uniforms(UB_fs_params, SG_RANGE(fs_params));



		for(const auto& o:objects) {
			renderObject(o);
		}

		renderQuad();

		sg_end_pass();
		sg_commit();
	}
};