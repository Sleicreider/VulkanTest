#version 450

layout(location = 0) in vec3 fragColor;  //interpolated color from vertex (position must match)


// out location 0 is differnt to in location
layout(location = 0) out vec4 outColor; //final out put color 

void main()
{
	outColor = vec4(fragColor, 1.f);
}