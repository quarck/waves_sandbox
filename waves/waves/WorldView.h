#pragma once 

#include <memory>

#define _USE_MATH_DEFINES
#include <math.h>

#include <GL/gl.h>			/* OpenGL header file */
#include <GL/glu.h>			/* OpenGL utilities header file */

#include "glText.h"

#include "World.h"

#include "Props.h"

namespace waves
{
	struct WorldViewDetails
	{
		int numActiveThreads;
		bool showDetailedcontrols;
		bool paused;

		WorldViewDetails(int nThr, bool p) 
			: numActiveThreads{ nThr }
			, showDetailedcontrols { false }
			, paused { p }
		{

		}
	};

    class WorldView
    {
		static constexpr uint32_t LABELS_BACKGROUND = 0xff000000;
		static constexpr uint32_t CONTROLS_LABEL_FOREGROUND = 0xff0f0f7f;
		static constexpr uint32_t RUGA_KOLORO = 0xff0f0fdf;
		static constexpr uint32_t VERDA_KOLORO = 0xff006f00u;
		static constexpr uint32_t CFG_CLR_FOREGROUND = 0xff9f004fu;

		static constexpr double LOCATION_SCALE{ 512.0 / props::ViewPortWidth }; 		
		
		World& _world;

		glText::Label _controlsLabel{ LABELS_BACKGROUND, CONTROLS_LABEL_FOREGROUND, "<?> - help" };

		glText::Label _controlsLabelDetailed{
			LABELS_BACKGROUND,
			{
				std::pair(RUGA_KOLORO, "<T> - toggle recording (dump png every 1024 frames)"),
				std::pair(RUGA_KOLORO, "<?> - help ON/OFF, <SPACE> - (un)pause, <esc> - quit"),
			}
		};

		glText::Label _iterAndCfgLabel{ LABELS_BACKGROUND, VERDA_KOLORO, "_TMP_" };

		glText::Label _pausedLabel{ LABELS_BACKGROUND, RUGA_KOLORO, "<< PAUSED >>" };
		
    public:

        WorldView(World& world)
            : _world(world)
        {
            Random rnd = Random();
		}

		void PrintControls(const WorldViewDetails& details) noexcept
		{
			glPushMatrix();

			glPixelZoom(1.f, 1.f);

			((details.showDetailedcontrols || details.paused) ? _controlsLabelDetailed : _controlsLabel)
				.DrawAt(-1.0, -0.99);

			if (details.paused)
				_pausedLabel.DrawAt(-0.2, 0);

			glPopMatrix();
		}

		void PrintStats(const WorldViewDetails& details, const std::string& currentObjectName) noexcept
		{
			glPushMatrix();

			glPixelZoom(1.f, 1.f);

			std::ostringstream rcfg;
			rcfg << "#THR: " << details.numActiveThreads;

			_iterAndCfgLabel.Update(
				LABELS_BACKGROUND,
				{ 
//					std::pair(VERDA_KOLORO, ostr.str()),
					std::pair(RUGA_KOLORO, currentObjectName), // rcfg.str()),
				});
			_iterAndCfgLabel.DrawAt(-1.0, 0.94);

			glPopMatrix();
		}

        void UpdateFrom(
			World& world,
			const WorldViewDetails& details, 
			bool hideControlsAndStats
		)  noexcept
        {
            glPushMatrix();

            // BG BEGIN
            glBegin(GL_TRIANGLES);

            glColor3f(0.0f, 0.0f, 0.0f);

            glIndexi(1); glVertex2f(1.0f, 1.0f);
            glIndexi(2); glVertex2f(-1.0f, 1.0f);
            glIndexi(3); glVertex2f(-1.0f, -1.0f);

            glIndexi(4); glVertex2f(1.0f, 1.0f);
            glIndexi(5); glVertex2f(1.0f, -1.0f);
            glIndexi(6); glVertex2f(-1.0f, -1.0f);

            glEnd();
            // BG END
			if (!hideControlsAndStats)
			{
				PrintControls(details);
			}


			glTranslatef(
				static_cast<GLfloat>(-1.0),
				static_cast<GLfloat>(-1.0),
				0.0f
			);

			glScalef(
				static_cast<GLfloat>(2.0 / waves::props::ViewPortWidth),
				static_cast<GLfloat>(2.0 / waves::props::ViewPortHeight),
				1.0f);

			if (waves::props::ViewPortHeight < waves::props::ViewPortWidth)
			{
				glTranslatef(
					+(waves::props::ViewPortWidth - waves::props::ViewPortHeight)/2.0,
					0.0,
					0.0f
				);
			}
			else
			{
				glTranslatef(
					0.0,
					-(waves::props::ViewPortWidth - waves::props::ViewPortHeight) / 2.0,
					0.0f
				);
			}

			auto min_size = static_cast<float>(std::min(waves::props::ViewPortWidth, waves::props::ViewPortHeight));

			const auto& medium = world.get_data();

			for (int x = 0; x < medium.width(); ++x)
			{
				for (int y = 0; y < medium.height(); ++y)
				{
					auto v = medium.at(x, y, 0);

					glPushMatrix();

					auto loc_x{ (float)x * min_size / medium.width() };
					auto loc_y{ (float)y * min_size / medium.height() };

					glTranslatef(loc_x, loc_y, 0.0);

					glBegin(GL_TRIANGLES);

					if (v > 500)
					{
						int i = 0;
					}

					float brightness = std::max(0.0, std::min(1.0, v / 1000.0));

					glColor3f(brightness, std::max(0.0f, 3.0f* brightness - 2.0f), 0.0);

					int idx = 0;

					const float sz = min_size / std::max(medium.width(), medium.height());

					glIndexi(++idx); glVertex2f(0.0f, 0.0f);
					glIndexi(++idx); glVertex2f(sz, 0.0f);
					glIndexi(++idx); glVertex2f(0.0f, sz);

					glIndexi(++idx); glVertex2f(sz, sz);
					glIndexi(++idx); glVertex2f(sz, 0.0f);
					glIndexi(++idx); glVertex2f(0.0f, sz);

					glEnd();

					glPopMatrix();

				}
			}

            glPopMatrix();
		}
    };
}
