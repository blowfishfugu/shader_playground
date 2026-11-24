#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <array>
#include <span>
#include <random>
#include <string>
#include <format>

constexpr int screenWidth = 1200;
constexpr int screenHeight = 800;

constexpr const char* pixelshader2d = R"fs(
#version 330

in vec2 fragTexCoord;
out vec4 finalColor;

uniform vec3 balls[32]; // x,y,radius
uniform vec3 ballColors[32]; // r, g, b (normalized [0.0..1.0])
uniform int ballCount;
uniform vec2 resolution;
uniform float time;           // Elapsed time in seconds

void main() {
    vec2 pixelPos = vec2(fragTexCoord.x * resolution.x, (1.0 - fragTexCoord.y) * resolution.y);
    
    float totalInfluence = 0.0;
    vec3 blendedColor = vec3(0.0);

    for (int i = 0; i < ballCount; i++) {
        vec2 diff = pixelPos - balls[i].xy;
        float distSq = dot(diff, diff);
        float radiusSq = balls[i].z * balls[i].z;
        float influence = radiusSq / max(distSq, 0.0001);
        totalInfluence += influence;
        blendedColor += ballColors[i] * influence;
    }

    if (totalInfluence > 0.0) {
        blendedColor /= totalInfluence;
    }

    //cel-shading
    float outlineMask   = smoothstep(0.82, 0.86, totalInfluence); // Outer
    float fillMask      = smoothstep(1.00, 1.04, totalInfluence); // Main
    float shadowMask    = smoothstep(1.18, 1.22, totalInfluence); // Inner
    float highlightMask = smoothstep(2.40, 2.50, totalInfluence); // Center dot

    vec3 outlineColor   = vec3(0.04, 0.04, 0.06);
    vec3 shadowColor    = blendedColor * 0.65;
    vec3 highlightColor = mix(blendedColor, vec3(1.0), 0.55);

    vec3 color = outlineColor;
    color = mix(color, shadowColor, fillMask);
    color = mix(color, blendedColor, shadowMask);
    color = mix(color, highlightColor, highlightMask);

    finalColor = vec4(color, outlineMask);
}
)fs";



struct Metaball {
	Vector2 pos{};
	float radius{};
};

int main(int argc, char** argv) {
	std::random_device rd;
	std::mt19937 mt{ rd() };
	std::uniform_real_distribution<float> rndX{ 100.0f, static_cast<float>(screenWidth - 100) };
	std::uniform_real_distribution<float> rndY{ 100.0f, static_cast<float>(screenHeight - 100) };
	std::uniform_real_distribution<float> rndVel{ -180.0f, 180.0f };
	std::uniform_real_distribution<float> rndRadius{ 25.0f, 120.0f };



	SetConfigFlags(FLAG_MSAA_4X_HINT
		| FLAG_VSYNC_HINT
	//	| FLAG_WINDOW_TRANSPARENT
	);
	InitWindow(screenWidth, screenHeight, "logo-balls");

	Shader shader = LoadShaderFromMemory(nullptr, pixelshader2d);
	enum eLoc :int { ballsLoc, colorsLoc, countLoc, resolutionLoc, timeLoc };

	std::array<int, 5> shaderLocs{
		GetShaderLocation(shader,"balls"),
		GetShaderLocation(shader,"ballColors"),
		GetShaderLocation(shader,"ballCount"),
		GetShaderLocation(shader,"resolution"),
		GetShaderLocation(shader, "time"),
	};


	//cartoon palette
	std::array<Vector3, 4> palette = {
		Vector3{ 255,  75, 110 }, // Coral Pink
		Vector3{ 50, 210, 255 }, // Electric Cyan
		Vector3{ 255, 210,  60 }, // Marigold Yellow
		Vector3{ 150,  90, 255 }  // Vivid Purple
	};
	//put to [0.0..1.0]
	for (Vector3& normalized : palette) {
		normalized.x = normalized.x / 255.0f;
		normalized.y = normalized.y / 255.0f;
		normalized.z = normalized.z / 255.0f;
	}

	//init balls
	int ballCount = 16; //visible balls
	constexpr int bufCount = 32; //available balls, ! has to match with shader-uniforms: balls[] and ballColors[]
	std::array<Metaball, bufCount> balls{}; //(pos.xy), radius
	std::array<Vector2, bufCount> velocities{}; //xy
	std::array<Vector3, bufCount> colorData{}; //rgb, normalized[0..1], alpha by shader

	for (int i = 0; i < ballCount; ++i) {
		Metaball& ball = balls[i];
		Vector2& vel = velocities[i];
		Vector3& col = colorData[i];
		ball.pos = { rndX(mt),rndY(mt) };
		vel = { rndVel(mt),rndVel(mt) };
		ball.radius = rndRadius(mt);
		col = palette[i % palette.size()];
	}

	SetShaderValue(shader, shaderLocs[countLoc], &ballCount, SHADER_UNIFORM_INT);
	SetShaderValueV(shader, shaderLocs[colorsLoc], colorData.data(), SHADER_UNIFORM_VEC3, ballCount);

	Vector2 vResolution{ (float)screenWidth,(float)screenHeight };
	SetShaderValue(shader, shaderLocs[resolutionLoc], &vResolution, SHADER_UNIFORM_VEC2);

	int currentWidth = screenWidth;
	int currentHeight = screenHeight;
	RenderTexture2D target = LoadRenderTexture(currentWidth, currentHeight);
	while (!WindowShouldClose()) {
		float elapsedTime = static_cast<float>(GetTime());
		SetShaderValue(shader, shaderLocs[timeLoc], &elapsedTime, SHADER_UNIFORM_FLOAT);

		//process inputs
		if (IsKeyPressed(KEY_F11)) {
			const int monitor = GetCurrentMonitor();
			if (!IsWindowFullscreen()) {
				Vector2 winPos = GetWindowPosition();
				for (int i = 0; i < ballCount; ++i) {
					Metaball& ball = balls[i];
					ball.pos.x += winPos.x;
					ball.pos.y += winPos.y;
				}

				currentWidth = GetMonitorWidth(monitor);
				currentHeight = GetMonitorHeight(monitor);
				SetWindowSize(currentWidth, currentHeight);
				ToggleFullscreen();
				UnloadRenderTexture(target);
				target = LoadRenderTexture(currentWidth, currentHeight);
				HideCursor();

			}
			else {
				currentWidth = screenWidth;
				currentHeight = screenHeight;
				ToggleFullscreen();
				SetWindowSize(currentWidth, currentHeight);

				Vector2 winPos = GetWindowPosition();
				for (int i = 0; i < ballCount; ++i) {
					Metaball& ball = balls[i];
					ball.pos.x -= winPos.x;
					ball.pos.y -= winPos.y;
				}

				UnloadRenderTexture(target);
				target = LoadRenderTexture(currentWidth, currentHeight);
				ShowCursor();
			}

			Vector2 newRes{
				(float)currentWidth,
				(float)currentHeight,
			};
			SetShaderValue(shader, shaderLocs[resolutionLoc], &newRes, SHADER_UNIFORM_VEC2);
		}

		float dt = GetFrameTime();
		//physics and borders
		for (int i = 0; i < ballCount; ++i) {
			Metaball& ball = balls[i];
			Vector2& vel = velocities[i];
			ball.pos = Vector2Add(ball.pos, Vector2Scale(velocities[i], dt));
			if (ball.pos.x < 0 && vel.x < 0) {
				vel.x *= -1.0f;
			}
			else if (ball.pos.x > currentWidth && vel.x > 0) {
				vel.x *= -1.0f;
			}

			if (ball.pos.y < 0 && vel.y < 0) {
				vel.y *= -1.0f;
			}
			else if (ball.pos.y > currentHeight && vel.y > 0) {
				vel.y *= -1.0f;
			}
		}

		SetShaderValueV(shader, shaderLocs[ballsLoc], balls.data(), SHADER_UNIFORM_VEC3, ballCount);

		BeginTextureMode(target);
		{
			ClearBackground(BLANK);
			BeginShaderMode(shader);
			{
				DrawTextureRec(target.texture, { 0.0f,0.0f,(float)currentWidth,(float)currentHeight }, Vector2{ 0,0 }, WHITE);
			}
			EndShaderMode();
		}
		EndTextureMode();

		//GenTextureMipmaps(&target.texture);
		//SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

		BeginDrawing();
		{
			ClearBackground(BLANK);
			DrawRectangleGradientV(0, 0, currentWidth, currentHeight / 2, BLANK, SKYBLUE);
			DrawRectangleGradientV(0, currentHeight / 2, currentWidth, currentHeight / 2, BLANK, LIGHTGRAY);


			DrawTextureRec(target.texture,
				{
					0.0f,0.0f,
					(float)target.texture.width,
					(float)target.texture.height
				},
				Vector2{ 0,0 }, WHITE); //<-retint all

			for (int i = 0; const Metaball& ball : balls) {
				const Vector3& color = colorData[i];
				DrawCircle(ball.pos.x, ball.pos.y, ball.radius * 0.1f, ColorFromNormalized(Vector4{ color.x,color.y,color.z,0.9f }));
#ifndef NDEBUG
				std::string txt = std::format("{:.2f} ; {:.2f}", ball.pos.x, ball.pos.y);
				DrawText(txt.c_str(), ball.pos.x, ball.pos.y, 12, BLACK);
#endif
				++i;
				if (i >= ballCount) { break; }
			}
#ifndef NDEBUG
			DrawFPS(10, 10);
#endif
		}
		EndDrawing();
	}

	UnloadRenderTexture(target);
	UnloadShader(shader);
	CloseWindow();

	return 0;
}