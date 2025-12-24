///////default pipeline shader //////////////////////////////

@vs vs
layout(binding=0) uniform vs_params{
	mat4 u_model;
	mat4 u_proj_view;
};

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec3 v_pos;
out vec3 v_norm;
out vec2 v_uv;

void main() {
	vec4 world_pos=u_model*vec4(pos, 1);
	v_pos=world_pos.xyz;
	v_norm=mat3(u_model)*norm;
	gl_Position=u_proj_view*world_pos;
	v_uv=uv;
}
@end

@fs fs
layout(binding=1) uniform fs_params{
	vec3 u_view_pos;
	int u_num_lights;
	vec4 u_light_pos[16];
	vec4 u_light_col[16];

};

in vec3 v_pos;
in vec3 v_norm;
in vec2 v_uv;

layout(binding=0) uniform texture2D u_tex;
layout(binding=0) uniform sampler u_smp;

out vec4 frag_col;

void main() {
	const float amb_mag=1;//0-1
	const vec3 amb_col=vec3(0.03);
	
	const float shininess=64;//32-256
	const float spec_mag=1;//0-1

	const float att_k0=1.0;
	const float att_k1=0.09;
	const float att_k2=0.032;

	//srgb->linear
	vec3 base_col_srgb=texture(sampler2D(u_tex, u_smp), v_uv).rgb;
	vec3 base_col=pow(base_col_srgb, vec3(2.2));

	vec3 n=normalize(v_norm);
	vec3 v=normalize(u_view_pos-v_pos);

	//start with ambient
	vec3 col=amb_col*base_col*amb_mag;
	for(int i=0; i<u_num_lights; i++) {
		vec3 l_pos=u_light_pos[i].xyz;
		vec3 l_col=u_light_col[i].rgb;

		vec3 l=l_pos-v_pos;
		float dist=length(l);
		l/=dist;

		//diffuse(Lambert)
		float n_dot_l=max(0, dot(n, l));
		vec3 diffuse=l_col*base_col*n_dot_l;

		//blinn-phong specular
		vec3 h=normalize(l+v);//half-vector
		float n_dot_h=max(0, dot(n, h));
		float spec_factor=pow(n_dot_h, shininess);
		vec3 specular=l_col*spec_factor*spec_mag;

		//attenuation (apply equally to all channels)
		float att=1/(att_k0+att_k1*dist+att_k2*dist*dist);

		col+=att*(diffuse+specular);
	}
	
	//linear->srgb
	vec3 col_srgb=pow(col, vec3(1/2.2));
	frag_col=vec4(col_srgb, 1);
}
@end

@program shd vs fs


///////3d billboarding animation shader //////////////////////////////

@vs vs_bba
layout(binding=0) uniform vs_bba_params{
	mat4 u_model;
	mat4 u_proj_view;
};

in vec3 pos;
in vec3 norm;
in vec2 uv;

out vec3 v_bba_pos;
out vec3 v_bba_norm;
out vec2 v_bba_uv;

void main() {
	vec4 world_pos=u_model*vec4(pos, 1);
	v_bba_pos=world_pos.xyz;
	v_bba_norm=mat3(u_model)*norm;
	gl_Position=u_proj_view*world_pos;
	v_bba_uv=uv;
}
@end

@fs fs_bba
layout(binding=1) uniform fs_bba_params{
    vec3 u_view_pos;
	vec2 u_tl;
	vec2 u_br;
	int u_num_lights;
	vec4 u_light_pos[16];
	vec4 u_light_col[16];
	
};

in vec3 v_bba_pos;
in vec3 v_bba_norm;
in vec2 v_bba_uv;

layout(binding=0) uniform texture2D u_bba_tex;
layout(binding=0) uniform sampler u_bba_smp;

out vec4 frag_col;

void main() {
	const float amb_mag=1;//0-1
	const vec3 amb_col=vec3(0.03);
	
	const float shininess=64;//32-256
	const float spec_mag=1;//0-1

	const float att_k0=1.0;
	const float att_k1=0.09;
	const float att_k2=0.032;

	//srgb->linear
	vec3 base_col_srgb=texture(sampler2D(u_bba_tex, u_bba_smp),u_tl + v_bba_uv * (u_br - u_tl)).rgb;
	vec3 base_col=pow(base_col_srgb, vec3(2.2));

	vec3 n=normalize(v_bba_norm);
	vec3 v=normalize(u_view_pos-v_bba_pos);

	//start with ambient
	vec3 col=amb_col*base_col*amb_mag;
	for(int i=0; i<u_num_lights; i++) {
		vec3 l_pos=u_light_pos[i].xyz;
		vec3 l_col=u_light_col[i].rgb;

		vec3 l=l_pos-v_bba_pos;
		float dist=length(l);
		l/=dist;

		//diffuse(Lambert)
		float n_dot_l=max(0, dot(n, l));
		vec3 diffuse=l_col*base_col*n_dot_l;

		//blinn-phong specular
		vec3 h=normalize(l+v);//half-vector
		float n_dot_h=max(0, dot(n, h));
		float spec_factor=pow(n_dot_h, shininess);
		vec3 specular=l_col*spec_factor*spec_mag;

		//attenuation (apply equally to all channels)
		float att=1/(att_k0+att_k1*dist+att_k2*dist*dist);

		col+=att*(diffuse+specular);
	}
	
	//linear->srgb
	vec3 col_srgb=pow(col, vec3(1/2.2));
	frag_col=vec4(col_srgb, 1);
}
@end

@program bba vs_bba fs_bba



//////////2d texture animation//////////////////////

@vs vs_texview

in vec2 v_pos;
in vec2 v_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(v_pos, .5, 1);
	uv.x = v_uv.x;
	uv.y = -v_uv.y;
}

@end

@fs fs_texview

layout(binding = 0) uniform texture2D texview_tex;
layout(binding = 0) uniform sampler texview_smp;

layout(binding = 0) uniform fs_texview_params
{
	int u_full_tex;
	vec2 u_tl;
	vec2 u_br;
};

in vec2 uv;

out vec4 frag_color;

void main()
{
	
	vec4 col = texture(sampler2D(texview_tex,texview_smp),u_tl + uv * (u_br - u_tl));
	frag_color = vec4(col.rgb, 1);
}

@end

@program texview vs_texview fs_texview

///////// static texture /////////////////////////////////////

@vs vs_bbview

in vec2 v_pos;
in vec2 v_uv;

out vec2 uv;

void main()
{
	gl_Position = vec4(v_pos, .5, 1);
	uv.x = v_uv.x;
	uv.y = -v_uv.y;
}

@end

@fs fs_bbview

layout(binding = 0) uniform texture2D bbview_tex;
layout(binding = 0) uniform sampler bbview_smp;


in vec2 uv;

out vec4 frag_color;

void main()
{
	frag_color=vec4(texture(sampler2D(bbview_tex, bbview_smp), uv).xxx, 1);
}

@end

@program bbview vs_bbview fs_bbview