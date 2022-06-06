#pragma once

// indices (locations) of  Queue families (if they exist at all)

struct QueueFamilyIndices
{
	int graphicsFamily = -1;	//location of graphics queue family

	//check if queue families are valid
	bool isValid() const
	{
		return graphicsFamily >= 0;
	}
};