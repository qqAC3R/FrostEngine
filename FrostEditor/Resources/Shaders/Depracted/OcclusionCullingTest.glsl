#version 450 core
layout(local_size_x = 64) in;

const vec2 MAX_DEPTH_IMAGE_SIZE = vec2(1024.f);

// for frustum culling
const int NUM_PLANES = 4;
const vec3 a[NUM_PLANES] = {
    vec3(1.f, 0.f, 0.f),
    vec3(-1.f, 0.f, 0.f),
    vec3(0.f, -1.f, 0.f),
    vec3(0.f, 1.f, 0.f)
};
const vec3 n[NUM_PLANES] = {
    vec3(-1.f, 0.f, 0.f),
    vec3(1.f, 0.f, 0.f),
    vec3(0.f, 1.f, 0.f),
    vec3(0.f, -1.f, 0.f)
};

// multi-draw indirect command
struct Mdi_cmd {
    uint idx_count;
    uint inst_count; // visibility
    uint idx_base;
    int vert_offset;
    uint inst_idx;
    float paddings[7];
};

struct Instance_properties {
    mat4 transform;
    vec3 bbmin;
    float padding;
    vec3 bbmax;
    float mtl_idx;
};

layout(set = 0, binding = 0) uniform sampler2D depth_dst_in;
layout(set = 0, binding = 1) readonly buffer Inst_data_buffer_in
{
    Instance_properties props[];
};
layout(set = 0, binding = 2)  buffer Mdi_cmd_buffer_out
{
    Mdi_cmd cmds[];
};

layout(set = 1, binding = 0) uniform UBO
{
    mat4 model;
    mat4 normal;
    mat4 view;
    mat4 projection_clip;
    float cam_near;
    float cam_far;
    vec2 resolution;
} ubo_in;

layout(push_constant) uniform Push_constant
{
    uint inst_total;
    uint use_occlusion_culling;
} consts;

uint cull_near_far(float view_z)
{
    return uint(step(view_z, - ubo_in.cam_near) *
		step(- view_z, ubo_in.cam_far));
}

uint cull_lrtb(vec3 ndc)
{
    uint res = 1;
    for (int i = 0; i < NUM_PLANES; i ++)
    {
	    float B = - dot(ndc - a[i], n[i]);
	    res &= uint(step(B, 0.f));
    }
    return res;
}

uint is_skybox(vec2 mn, vec2 mx )
{
    return uint(step(dot(mn, mx), 0.f));
}

void main()
{
    uint idx = gl_GlobalInvocationID.x % consts.inst_total;
    mat4 model_view = ubo_in.view * ubo_in.model * props[idx].transform;

    // the occlusion culling method is based on:
    // https://interplayoflight.wordpress.com/2017/11/15/experiments-in-gpu-based-occlusion-culling

    vec3 bbmin = props[idx].bbmin;
    vec3 bbmax = props[idx].bbmax;
    vec3 bbsize = bbmax - bbmin;

    const int CORNER_COUNT = 8;
    vec3 corners[CORNER_COUNT] = {
	    bbmin,
	    bbmin + vec3(bbsize.x, 0.f, 0.f),
	    bbmin + vec3(0.f, bbsize.y, 0.f),
	    bbmin + vec3(0.f, 0.f, bbsize.z),
	    bbmin + vec3(bbsize.xy, 0.f),
	    bbmin + vec3(0.f, bbsize.yz),
	    bbmin + vec3(bbsize.x, 0.f, bbsize.z),
	    bbmax
    };

    vec2 ndc_min = vec2(1.f);
    vec2 ndc_max = vec2(-1.f);
    float z_min = 1.f;

    uint res = 0;
    for (int i = 0; i < CORNER_COUNT; i ++)
    {
	    // cull near far
	    vec4 view_pos = model_view * vec4(corners[i], 1.f);
	    uint nf_res = cull_near_far(view_pos.z);

	    // cull left right top bottom
	    vec4 clip_pos = ubo_in.projection_clip * view_pos;
	    vec3 ndc_pos = clip_pos.xyz / clip_pos.w;

	    // clip objects behind near plane
	    ndc_pos.z *= step(view_pos.z, ubo_in.cam_near);

	    uint lrtb_res = cull_lrtb(ndc_pos);

	    ndc_pos.xy = max(vec2(-1.f), min(vec2(1.f), ndc_pos.xy));
	    ndc_pos.z = max(0.f, min(1.f, ndc_pos.z));

	    ndc_min = min(ndc_min, ndc_pos.xy);
	    ndc_max = max(ndc_max, ndc_pos.xy);
	    z_min = min(z_min, ndc_pos.z);

	    res = max(res, nf_res * lrtb_res);
    }
    res = max(res, is_skybox(ndc_min, ndc_max));

    vec2 viewport = ubo_in.resolution;
    vec2 scr_pos_min = (ndc_min * .5f + .5f) * viewport;
    vec2 scr_pos_max = (ndc_max * .5f + .5f) * viewport;
    vec2 scr_rect = (ndc_max - ndc_min) * .5f * viewport;
    float scr_size = max(scr_rect.x, scr_rect.y);

    int mip = int(ceil(log2(scr_size)));
    uvec2 dim = (uvec2(scr_pos_max) >> mip) - (uvec2(scr_pos_min) >> mip);
    int use_lower = int(step(dim.x, 2.f) * step(dim.y, 2.f));
    mip = use_lower * max(0, mip - 1) + (1 - use_lower) * mip;

    vec2 uv_scale = vec2(uvec2(ubo_in.resolution) >> mip) / ubo_in.resolution / vec2(1024 >> mip);
    vec2 uv_min = scr_pos_min * uv_scale;
    vec2 uv_max = scr_pos_max * uv_scale;

    vec2 coords[4] = {
	    uv_min,
	    vec2(uv_min.x, uv_max.y),
	    vec2(uv_max.x, uv_min.y),
	    uv_max
    };

    float scene_z = 0.f;
    for (int i = 0; i < 4; i ++) {
	    scene_z = max(scene_z, textureLod(depth_dst_in, coords[i], mip).r);
    }

    // cull occluder
    uint res_occluder = 1 - uint(step(scene_z, z_min));
    res *= max(1 - consts.use_occlusion_culling, res_occluder);

    cmds[idx].inst_count = res;
}