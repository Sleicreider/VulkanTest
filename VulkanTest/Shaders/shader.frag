#version 450

// out location 0 is differnt to in location
layout(location = 0) out vec4 outColor; //final out put color 
layout(location = 0) in vec3 fragCol;

void main()
{
	outColor = vec4(fragCol, 1.f);
}