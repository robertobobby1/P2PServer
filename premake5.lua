outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

workspace "Server"
	configurations
	{
		"Debug",
		"Release",
		"Dist"
	}

	filter "system:macosx"
		architecture "universal"

	filter "system:windows"
		architecture "x86_64"

	filter "system:linux"
		architecture "x86_64"

	project "Server"
		kind "ConsoleApp"
		language "C++"
		cppdialect "C++20"
	
		targetdir ("%{wks.location}/bin/" .. outputdir .. "/")
		objdir ("%{wks.location}/bin/bin-int/" .. outputdir .. "/%{prj.name}")

		files
		{
			"%{wks.location}/NoBiggyServer/Source/**.h",
			"%{wks.location}/NoBiggyServer/Source/**.cpp",
			"%{wks.location}/NoBiggyServer/Tests/**.h",
			"%{wks.location}/NoBiggyServer/Tests/**.cpp",
		}
		includedirs
		{
			"%{wks.location}/NoBiggyServer/Source",
			"%{wks.location}/NoBiggyServer/Tests",
		}

		filter "configurations:Debug"
			defines "RC_DEBUG"
			runtime "Debug"
			symbols "on"

		filter "configurations:Release"
			defines "RC_RELEASE"
			runtime "Release"
			optimize "on"
