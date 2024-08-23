outputdir = "%{cfg.buildcfg}-%{cfg.system}"

workspace "Server"
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	project "P2PServer"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++20"
	
		targetdir ("%{wks.location}/bin/" .. outputdir .. "/")
		objdir ("%{wks.location}/bin/bin-int/" .. outputdir .. "/%{prj.name}")

		files
		{
			"%{wks.location}/P2PServer/Source/**.h",
			"%{wks.location}/P2PServer/Source/**.cpp",
			"%{wks.location}/P2PServer/Tests/**.h",
			"%{wks.location}/P2PServer/Tests/**.cpp",
		}
		includedirs
		{
			"%{wks.location}/P2PServer/Source",
			"%{wks.location}/P2PServer/Tests",
		}
