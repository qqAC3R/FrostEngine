#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 a_Color;

layout(location = 0) out vec4 color;


// Rays
struct ray
{
	vec3 origin;
	vec3 direction;
};
vec3 at(ray r, float t)
{
	return r.origin + t * r.direction;
}


// Vector
float length_vec(vec3 v)
{
	return v.x*v.x + v.y*v.y + v.z*v.z;
}

vec3 unit_vector(vec3 v)
{
	return v / length_vec(v);
}

float hit_sphere(vec3 center, float radius, ray r) {
    vec3 oc = r.origin - center;
    float a = dot(r.direction, r.direction);
    //float b = 2.0 * dot(oc, r.direction);
	float half_b = dot(oc, r.direction);
    float c = dot(oc, oc) - radius*radius;
    float discriminant = half_b*half_b - a*c;

    if (discriminant < 0)
	{
		return -1.0;
	}
	else
	{
		return (-half_b -sqrt(discriminant)) / a;
	}
}

vec3 ray_color(ray r)
{
	float t = hit_sphere(vec3(0.0, 0.0, -1.0), 0.3, r);
	if(t > 0.0)
	{
		vec3 N = unit_vector(at(r, t) - vec3(0, 0, -1));
		return 0.5 * vec3(N.x+1.0, N.y+1.0, N.z+1.0);
	}
	vec3 unit_direction = unit_vector(r.direction);
    t = 0.5*(unit_direction.y + 1.0);
    return (1.0-t) * vec3(1.0, 1.0, 1.0) + t * vec3(0.5, 0.7, 1.0);
}

void main()
{
	const int width = 800;
	const int height = 600;
	const float aspect_ratio = 4.0 / 3.0;

	vec2 screen = vec2(ceil(gl_FragCoord.x), ceil(gl_FragCoord.y));

	float viewport_height = 2.0;
	float viewport_width = aspect_ratio * viewport_height;
	float focal_length = 1.0;

	vec3 origin = vec3(0.0);
	vec3 horizontal = vec3(viewport_width, 0, 0);
	vec3 vertical = vec3(0, viewport_height, 0);
    vec3 lower_left_corner = origin - horizontal/2 - vertical/2 - vec3(0, 0, focal_length);


	float u = float(screen.x) / float(width);
	float v = float(screen.y) / float(height);

	ray r;
	r.origin = origin;
	r.direction = lower_left_corner + u*horizontal + v*vertical - origin;

	vec3 pixel = ray_color(r);

	//color = vec4(u, v, 0.5, 1.0);
	//color = vec4(screen, 1.0, 1.0);
	color = vec4(pixel, 1.0);

	//color = vec4(a_Color, 1.0);
}