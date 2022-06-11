#version 450  // glsl 4.5

layout(location = 0) out vec3 fragColor; //output color of vertex

// triangle vertex positions (later in vertex buffer)
vec3 positions[3] = 
{
	vec3(0.f, -.4f, 0.f),
	vec3(.4f, .4f, 0.f),
	vec3(-.4f, .4f, 0.f)
};

//triangle vertex colors
vec3 colors[3] = 
{
	vec3(1.f, 0.f, 0.f),
	vec3(0.f, 1.f, 0.f),
	vec3(0.f, 0.f, 1.f)
};

void main()
{
	gl_Position = vec4(positions[gl_VertexIndex], 1.f);
	fragColor = colors[gl_VertexIndex];
}
