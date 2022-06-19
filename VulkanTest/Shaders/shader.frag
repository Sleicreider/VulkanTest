#version 450

// out location 0 is differnt to in location
layout(location = 0) out vec4 outColor; //final out put color 

void main()
{
	outColor = vec4(1.f, 0.f, 0.f, 1.f);
}