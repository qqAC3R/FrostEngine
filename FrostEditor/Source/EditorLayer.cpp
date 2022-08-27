#if 0
#include "frostpch.h"
#include "EditorLayer.h"

#include "imgui.h"

namespace Frost
{



	EditorLayer::EditorLayer()
		: Layer("Example"), m_Camera(70, 16.0f / 9.0f, 0.1f, 100.0f, Frost::CameraMode::ORBIT)
	{

	}

	void EditorLayer::OnAttach()
	{
		m_PlaneMesh = Mesh::Load("assets/media/scenes/plane.obj");
		m_VikingMesh = Mesh::Load("assets/models/stanford-bunny.fbx");
	}

	void EditorLayer::OnUpdate(Frost::Timestep ts)
	{
		m_Camera.OnUpdate(ts);

		Renderer::BeginRenderPass(m_Camera);

		glm::mat4 planeTransform = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
		Renderer::Submit(m_PlaneMesh, planeTransform);


		glm::mat4 vikingTransform = glm::translate(glm::mat4(1.0f), m_VikingMeshPosition) * glm::scale(glm::mat4(1.0f), glm::vec3(0.01f));
		Renderer::Submit(m_VikingMesh, vikingTransform);



		Renderer::EndRenderPass();
	}

	void EditorLayer::OnDetach()
	{
		m_PlaneMesh->Destroy();
		m_VikingMesh->Destroy();
	}

	void EditorLayer::OnEvent(Frost::Event& event)
	{
		m_Camera.OnEvent(event);
	}

	void EditorLayer::OnImGuiRender()
	{
		bool showWindow = true;
		ImGui::ShowDemoWindow(&showWindow);


		static bool dockSpaceOpen = true;

		static bool opt_fullscreen = true;
		static bool opt_padding = false;

		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
		if (opt_fullscreen)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->GetWorkPos());
			ImGui::SetNextWindowSize(viewport->GetWorkSize());
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
			window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
		}
		else
			dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		if (!opt_padding)
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));


		ImGui::Begin("DockSpace Demo", &dockSpaceOpen, window_flags);
		if (!opt_padding)
			ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);


		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Exit"))
				{
					//Application::Get().Close();
				}

				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}


		Frost::UI::Begin("Window");
		Frost::UI::End();

		ImGui::End();
	}

	void EditorLayer::OnResize()
	{

	}

}
#endif
#if 0
ImGui::Begin("Icons");

ImGui::Text(ICON_GLASS);
ImGui::Text(ICON_MUSIC);
ImGui::Text(ICON_SEARCH);
ImGui::Text(ICON_ENVELOPE_O);
ImGui::Text(ICON_HEART);
ImGui::Text(ICON_STAR);
ImGui::Text(ICON_STAR_O);
ImGui::Text(ICON_USER);
ImGui::Text(ICON_FILM);
ImGui::Text(ICON_TH_LARGE);
ImGui::Text(ICON_TH);
ImGui::Text(ICON_TH_LIST);
ImGui::Text(ICON_CHECK);
ImGui::Text(ICON_TIMES);
ImGui::Text(ICON_SEARCH_PLUS);
ImGui::Text(ICON_SEARCH_MINUS);
ImGui::Text(ICON_POWER_OFF);
ImGui::Text(ICON_SIGNAL);
ImGui::Text(ICON_COG);
ImGui::Text(ICON_TRASH_O);
ImGui::Text(ICON_HOME);
ImGui::Text(ICON_FILE_O);
ImGui::Text(ICON_CLOCK_O);
ImGui::Text(ICON_ROAD);
ImGui::Text(ICON_DOWNLOAD);
ImGui::Text(ICON_ARROW_CIRCLE_O_DOWN);
ImGui::Text(ICON_ARROW_CIRCLE_O_UP);
ImGui::Text(ICON_INBOX);
ImGui::Text(ICON_PLAY_CIRCLE_O);
ImGui::Text(ICON_REPEAT);
ImGui::Text(ICON_REFRESH);
ImGui::Text(ICON_LIST_ALT);
ImGui::Text(ICON_LOCK);
ImGui::Text(ICON_FLAG);
ImGui::Text(ICON_HEADPHONES);
ImGui::Text(ICON_VOLUME_OFF);
ImGui::Text(ICON_VOLUME_DOWN);
ImGui::Text(ICON_VOLUME_UP);
ImGui::Text(ICON_QRCODE);
ImGui::Text(ICON_BARCODE);
ImGui::Text(ICON_TAG);
ImGui::Text(ICON_TAGS);
ImGui::Text(ICON_BOOK);
ImGui::Text(ICON_BOOKMARK);
ImGui::Text(ICON_PRINT);
ImGui::Text(ICON_CAMERA);
ImGui::Text(ICON_FONT);
ImGui::Text(ICON_BOLD);
ImGui::Text(ICON_ITALIC);
ImGui::Text(ICON_TEXT_HEIGHT);
ImGui::Text(ICON_TEXT_WIDTH);
ImGui::Text(ICON_ALIGN_LEFT);
ImGui::Text(ICON_ALIGN_CENTER);
ImGui::Text(ICON_ALIGN_RIGHT);
ImGui::Text(ICON_ALIGN_JUSTIFY);
ImGui::Text(ICON_LIST);
ImGui::Text(ICON_OUTDENT);
ImGui::Text(ICON_INDENT);
ImGui::Text(ICON_VIDEO_CAMERA);
ImGui::Text(ICON_PICTURE_O);
ImGui::Text(ICON_PENCIL);
ImGui::Text(ICON_MAP_MARKER);
ImGui::Text(ICON_ADJUST);
ImGui::Text(ICON_TINT);
ImGui::Text(ICON_PENCIL_SQUARE_O);
ImGui::Text(ICON_SHARE_SQUARE_O);
ImGui::Text(ICON_CHECK_SQUARE_O);
ImGui::Text(ICON_ARROWS);
ImGui::Text(ICON_STEP_BACKWARD);
ImGui::Text(ICON_FAST_BACKWARD);
ImGui::Text(ICON_BACKWARD);
ImGui::Text(ICON_PLAY);
ImGui::Text(ICON_PAUSE);
ImGui::Text(ICON_STOP);
ImGui::Text(ICON_FORWARD);
ImGui::Text(ICON_FAST_FORWARD);
ImGui::Text(ICON_STEP_FORWARD);
ImGui::Text(ICON_EJECT);
ImGui::Text(ICON_CHEVRON_LEFT);
ImGui::Text(ICON_CHEVRON_RIGHT);
ImGui::Text(ICON_PLUS_CIRCLE);
ImGui::Text(ICON_MINUS_CIRCLE);
ImGui::Text(ICON_TIMES_CIRCLE);
ImGui::Text(ICON_CHECK_CIRCLE);
ImGui::Text(ICON_QUESTION_CIRCLE);
ImGui::Text(ICON_INFO_CIRCLE);
ImGui::Text(ICON_CROSSHAIRS);
ImGui::Text(ICON_TIMES_CIRCLE_O);
ImGui::Text(ICON_CHECK_CIRCLE_O);
ImGui::Text(ICON_BAN);
ImGui::Text(ICON_ARROW_LEFT);
ImGui::Text(ICON_ARROW_RIGHT);
ImGui::Text(ICON_ARROW_UP);
ImGui::Text(ICON_ARROW_DOWN);
ImGui::Text(ICON_SHARE);
ImGui::Text(ICON_EXPAND);
ImGui::Text(ICON_COMPRESS);
ImGui::Text(ICON_PLUS);
ImGui::Text(ICON_MINUS);
ImGui::Text(ICON_ASTERISK);
ImGui::Text(ICON_EXCLAMATION_CIRCLE);
ImGui::Text(ICON_GIFT);
ImGui::Text(ICON_LEAF);
ImGui::Text(ICON_FIRE);
ImGui::Text(ICON_EYE);
ImGui::Text(ICON_EYE_SLASH);
ImGui::Text(ICON_EXCLAMATION_TRIANGLE);
ImGui::Text(ICON_PLANE);
ImGui::Text(ICON_CALENDAR);
ImGui::Text(ICON_RANDOM);
ImGui::Text(ICON_COMMENT);
ImGui::Text(ICON_MAGNET);
ImGui::Text(ICON_CHEVRON_UP);
ImGui::Text(ICON_CHEVRON_DOWN);
ImGui::Text(ICON_RETWEET);
ImGui::Text(ICON_SHOPPING_CART);
ImGui::Text(ICON_FOLDER);
ImGui::Text(ICON_FOLDER_OPEN);
ImGui::Text(ICON_ARROWS_V);
ImGui::Text(ICON_ARROWS_H);
ImGui::Text(ICON_BAR_CHART);
ImGui::Text(ICON_TWITTER_SQUARE);
ImGui::Text(ICON_FACEBOOK_SQUARE);
ImGui::Text(ICON_CAMERA_RETRO);
ImGui::Text(ICON_KEY);
ImGui::Text(ICON_COGS);
ImGui::Text(ICON_COMMENTS);
ImGui::Text(ICON_THUMBS_O_UP);
ImGui::Text(ICON_THUMBS_O_DOWN);
ImGui::Text(ICON_STAR_HALF);
ImGui::Text(ICON_HEART_O);
ImGui::Text(ICON_SIGN_OUT);
ImGui::Text(ICON_LINKEDIN_SQUARE);
ImGui::Text(ICON_THUMB_TACK);
ImGui::Text(ICON_EXTERNAL_LINK);
ImGui::Text(ICON_SIGN_IN);
ImGui::Text(ICON_TROPHY);
ImGui::Text(ICON_GITHUB_SQUARE);
ImGui::Text(ICON_UPLOAD);
ImGui::Text(ICON_LEMON_O);
ImGui::Text(ICON_PHONE);
ImGui::Text(ICON_SQUARE_O);
ImGui::Text(ICON_BOOKMARK_O);
ImGui::Text(ICON_PHONE_SQUARE);
ImGui::Text(ICON_TWITTER);
ImGui::Text(ICON_FACEBOOK);
ImGui::Text(ICON_GITHUB);
ImGui::Text(ICON_UNLOCK);
ImGui::Text(ICON_CREDIT_CARD);
ImGui::Text(ICON_RSS);
ImGui::Text(ICON_HDD_O);
ImGui::Text(ICON_BULLHORN);
ImGui::Text(ICON_BELL);
ImGui::Text(ICON_CERTIFICATE);
ImGui::Text(ICON_HAND_O_RIGHT);
ImGui::Text(ICON_HAND_O_LEFT);
ImGui::Text(ICON_HAND_O_UP);
ImGui::Text(ICON_HAND_O_DOWN);
ImGui::Text(ICON_ARROW_CIRCLE_LEFT);
ImGui::Text(ICON_ARROW_CIRCLE_RIGHT);
ImGui::Text(ICON_ARROW_CIRCLE_UP);
ImGui::Text(ICON_ARROW_CIRCLE_DOWN);
ImGui::Text(ICON_GLOBE);
ImGui::Text(ICON_GLOBE_E);
ImGui::Text(ICON_GLOBE_W);
ImGui::Text(ICON_WRENCH);
ImGui::Text(ICON_TASKS);
ImGui::Text(ICON_FILTER);
ImGui::Text(ICON_BRIEFCASE);
ImGui::Text(ICON_ARROWS_ALT);
ImGui::Text(ICON_USERS);
ImGui::Text(ICON_LINK);
ImGui::Text(ICON_CLOUD);
ImGui::Text(ICON_FLASK);
ImGui::Text(ICON_SCISSORS);
ImGui::Text(ICON_FILES_O);
ImGui::Text(ICON_PAPERCLIP);
ImGui::Text(ICON_FLOPPY_O);
ImGui::Text(ICON_SQUARE);
ImGui::Text(ICON_BARS);
ImGui::Text(ICON_LIST_UL);
ImGui::Text(ICON_LIST_OL);
ImGui::Text(ICON_STRIKETHROUGH);
ImGui::Text(ICON_UNDERLINE);
ImGui::Text(ICON_TABLE);
ImGui::Text(ICON_MAGIC);
ImGui::Text(ICON_TRUCK);
ImGui::Text(ICON_PINTEREST);
ImGui::Text(ICON_PINTEREST_SQUARE);
ImGui::Text(ICON_GOOGLE_PLUS_SQUARE);
ImGui::Text(ICON_GOOGLE_PLUS);
ImGui::Text(ICON_MONEY);
ImGui::Text(ICON_CARET_DOWN);
ImGui::Text(ICON_CARET_UP);
ImGui::Text(ICON_CARET_LEFT);
ImGui::Text(ICON_CARET_RIGHT);
ImGui::Text(ICON_COLUMNS);
ImGui::Text(ICON_SORT);
ImGui::Text(ICON_SORT_DESC);
ImGui::Text(ICON_SORT_ASC);
ImGui::Text(ICON_ENVELOPE);
ImGui::Text(ICON_LINKEDIN);
ImGui::Text(ICON_UNDO);
ImGui::Text(ICON_GAVEL);
ImGui::Text(ICON_TACHOMETER);
ImGui::Text(ICON_COMMENT_O);
ImGui::Text(ICON_COMMENTS_O);
ImGui::Text(ICON_BOLT);
ImGui::Text(ICON_SITEMAP);
ImGui::Text(ICON_UMBRELLA);
ImGui::Text(ICON_CLIPBOARD);
ImGui::Text(ICON_LIGHTBULB_O);
ImGui::Text(ICON_EXCHANGE);
ImGui::Text(ICON_CLOUD_DOWNLOAD);
ImGui::Text(ICON_CLOUD_UPLOAD);
ImGui::Text(ICON_USER_MD);
ImGui::Text(ICON_STETHOSCOPE);
ImGui::Text(ICON_SUITCASE);
ImGui::Text(ICON_BELL_O);
ImGui::Text(ICON_COFFEE);
ImGui::Text(ICON_CUTLERY);
ImGui::Text(ICON_FILE_TEXT_O);
ImGui::Text(ICON_BUILDING_O);
ImGui::Text(ICON_HOSPITAL_O);
ImGui::Text(ICON_AMBULANCE);
ImGui::Text(ICON_MEDKIT);
ImGui::Text(ICON_FIGHTER_JET);
ImGui::Text(ICON_BEER);
ImGui::Text(ICON_H_SQUARE);
ImGui::Text(ICON_PLUS_SQUARE);
ImGui::Text(ICON_ANGLE_DOUBLE_LEFT);
ImGui::Text(ICON_ANGLE_DOUBLE_RIGHT);
ImGui::Text(ICON_ANGLE_DOUBLE_UP);
ImGui::Text(ICON_ANGLE_DOUBLE_DOWN);
ImGui::Text(ICON_ANGLE_LEFT);
ImGui::Text(ICON_ANGLE_RIGHT);
ImGui::Text(ICON_ANGLE_UP);
ImGui::Text(ICON_ANGLE_DOWN);
ImGui::Text(ICON_DESKTOP);
ImGui::Text(ICON_LAPTOP);
ImGui::Text(ICON_TABLET);
ImGui::Text(ICON_MOBILE);
ImGui::Text(ICON_CIRCLE_O);
ImGui::Text(ICON_QUOTE_LEFT);
ImGui::Text(ICON_QUOTE_RIGHT);
ImGui::Text(ICON_SPINNER);
ImGui::Text(ICON_CIRCLE);
ImGui::Text(ICON_REPLY);
ImGui::Text(ICON_GITHUB_ALT);
ImGui::Text(ICON_FOLDER_O);
ImGui::Text(ICON_FOLDER_OPEN_O);
ImGui::Text(ICON_SMILE_O);
ImGui::Text(ICON_FROWN_O);
ImGui::Text(ICON_MEH_O);
ImGui::Text(ICON_GAMEPAD);
ImGui::Text(ICON_KEYBOARD_O);
ImGui::Text(ICON_FLAG_O);
ImGui::Text(ICON_FLAG_CHECKERED);
ImGui::Text(ICON_TERMINAL);
ImGui::Text(ICON_CODE);
ImGui::Text(ICON_REPLY_ALL);
ImGui::Text(ICON_STAR_HALF_O);
ImGui::Text(ICON_LOCATION_ARROW);
ImGui::Text(ICON_CROP);
ImGui::Text(ICON_CODE_FORK);
ImGui::Text(ICON_CHAIN_BROKEN);
ImGui::Text(ICON_QUESTION);
ImGui::Text(ICON_INFO);
ImGui::Text(ICON_EXCLAMATION);
ImGui::Text(ICON_SUPERSCRIPT);
ImGui::Text(ICON_SUBSCRIPT);
ImGui::Text(ICON_ERASER);
ImGui::Text(ICON_PUZZLE_PIECE);
ImGui::Text(ICON_MICROPHONE);
ImGui::Text(ICON_MICROPHONE_SLASH);
ImGui::Text(ICON_SHIELD);
ImGui::Text(ICON_CALENDAR_O);
ImGui::Text(ICON_FIRE_EXTINGUISHER);
ImGui::Text(ICON_ROCKET);
ImGui::Text(ICON_MAXCDN);
ImGui::Text(ICON_CHEVRON_CIRCLE_LEFT);
ImGui::Text(ICON_CHEVRON_CIRCLE_RIGHT);
ImGui::Text(ICON_CHEVRON_CIRCLE_UP);
ImGui::Text(ICON_CHEVRON_CIRCLE_DOWN);
ImGui::Text(ICON_HTML5);
ImGui::Text(ICON_CSS3);
ImGui::Text(ICON_ANCHOR);
ImGui::Text(ICON_UNLOCK_ALT);
ImGui::Text(ICON_BULLSEYE);
ImGui::Text(ICON_ELLIPSIS_H);
ImGui::Text(ICON_ELLIPSIS_V);
ImGui::Text(ICON_RSS_SQUARE);
ImGui::Text(ICON_PLAY_CIRCLE);
ImGui::Text(ICON_TICKET);
ImGui::Text(ICON_MINUS_SQUARE);
ImGui::Text(ICON_MINUS_SQUARE_O);
ImGui::Text(ICON_LEVEL_UP);
ImGui::Text(ICON_LEVEL_DOWN);
ImGui::Text(ICON_CHECK_SQUARE);
ImGui::Text(ICON_PENCIL_SQUARE);
ImGui::Text(ICON_EXTERNAL_LINK_SQUARE);
ImGui::Text(ICON_SHARE_SQUARE);
ImGui::Text(ICON_COMPASS);
ImGui::Text(ICON_CARET_SQUARE_O_DOWN);
ImGui::Text(ICON_CARET_SQUARE_O_UP);
ImGui::Text(ICON_CARET_SQUARE_O_RIGHT);
ImGui::Text(ICON_EUR);
ImGui::Text(ICON_GBP);
ImGui::Text(ICON_USD);
ImGui::Text(ICON_INR);
ImGui::Text(ICON_JPY);
ImGui::Text(ICON_RUB);
ImGui::Text(ICON_KRW);
ImGui::Text(ICON_BTC);
ImGui::Text(ICON_FILE);
ImGui::Text(ICON_FILE_TEXT);
ImGui::Text(ICON_SORT_ALPHA_ASC);
ImGui::Text(ICON_SORT_ALPHA_DESC);
ImGui::Text(ICON_SORT_AMOUNT_ASC);
ImGui::Text(ICON_SORT_AMOUNT_DESC);
ImGui::Text(ICON_SORT_NUMERIC_ASC);
ImGui::Text(ICON_SORT_NUMERIC_DESC);
ImGui::Text(ICON_THUMBS_UP);
ImGui::Text(ICON_THUMBS_DOWN);
ImGui::Text(ICON_YOUTUBE_SQUARE);
ImGui::Text(ICON_YOUTUBE);
ImGui::Text(ICON_XING);
ImGui::Text(ICON_XING_SQUARE);
ImGui::Text(ICON_YOUTUBE_PLAY);
ImGui::Text(ICON_DROPBOX);
ImGui::Text(ICON_STACK_OVERFLOW);
ImGui::Text(ICON_INSTAGRAM);
ImGui::Text(ICON_FLICKR);
ImGui::Text(ICON_ADN);
ImGui::Text(ICON_BITBUCKET);
ImGui::Text(ICON_BITBUCKET_SQUARE);
ImGui::Text(ICON_TUMBLR);
ImGui::Text(ICON_TUMBLR_SQUARE);
ImGui::Text(ICON_LONG_ARROW_DOWN);
ImGui::Text(ICON_LONG_ARROW_UP);
ImGui::Text(ICON_LONG_ARROW_LEFT);
ImGui::Text(ICON_LONG_ARROW_RIGHT);
ImGui::Text(ICON_APPLE);
ImGui::Text(ICON_WINDOWS);
ImGui::Text(ICON_ANDROID);
ImGui::Text(ICON_LINUX);
ImGui::Text(ICON_DRIBBBLE);
ImGui::Text(ICON_SKYPE);
ImGui::Text(ICON_FOURSQUARE);
ImGui::Text(ICON_TRELLO);
ImGui::Text(ICON_FEMALE);
ImGui::Text(ICON_MALE);
ImGui::Text(ICON_GRATIPAY);
ImGui::Text(ICON_SUN_O);
ImGui::Text(ICON_MOON_O);
ImGui::Text(ICON_ARCHIVE);
ImGui::Text(ICON_BUG);
ImGui::Text(ICON_VK);
ImGui::Text(ICON_WEIBO);
ImGui::Text(ICON_RENREN);
ImGui::Text(ICON_PAGELINES);
ImGui::Text(ICON_STACK_EXCHANGE);
ImGui::Text(ICON_ARROW_CIRCLE_O_RIGHT);
ImGui::Text(ICON_ARROW_CIRCLE_O_LEFT);
ImGui::Text(ICON_CARET_SQUARE_O_LEFT);
ImGui::Text(ICON_DOT_CIRCLE_O);
ImGui::Text(ICON_WHEELCHAIR);
ImGui::Text(ICON_VIMEO_SQUARE);
ImGui::Text(ICON_TRY);
ImGui::Text(ICON_PLUS_SQUARE_O);
ImGui::Text(ICON_SPACE_SHUTTLE);
ImGui::Text(ICON_SLACK);
ImGui::Text(ICON_ENVELOPE_SQUARE);
ImGui::Text(ICON_WORDPRESS);
ImGui::Text(ICON_OPENID);
ImGui::Text(ICON_UNIVERSITY);
ImGui::Text(ICON_GRADUATION_CAP);
ImGui::Text(ICON_YAHOO);
ImGui::Text(ICON_GOOGLE);
ImGui::Text(ICON_REDDIT);
ImGui::Text(ICON_REDDIT_SQUARE);
ImGui::Text(ICON_STUMBLEUPON_CIRCLE);
ImGui::Text(ICON_STUMBLEUPON);
ImGui::Text(ICON_DELICIOUS);
ImGui::Text(ICON_DIGG);
ImGui::Text(ICON_DRUPAL);
ImGui::Text(ICON_JOOMLA);
ImGui::Text(ICON_LANGUAGE);
ImGui::Text(ICON_FAX);
ImGui::Text(ICON_BUILDING);
ImGui::Text(ICON_CHILD);
ImGui::Text(ICON_PAW);
ImGui::Text(ICON_SPOON);
ImGui::Text(ICON_CUBE);
ImGui::Text(ICON_CUBES);
ImGui::Text(ICON_BEHANCE);
ImGui::Text(ICON_BEHANCE_SQUARE);
ImGui::Text(ICON_STEAM);
ImGui::Text(ICON_STEAM_SQUARE);
ImGui::Text(ICON_RECYCLE);
ImGui::Text(ICON_CAR);
ImGui::Text(ICON_TAXI);
ImGui::Text(ICON_TREE);
ImGui::Text(ICON_SPOTIFY);
ImGui::Text(ICON_DEVIANTART);
ImGui::Text(ICON_SOUNDCLOUD);
ImGui::Text(ICON_DATABASE);
ImGui::Text(ICON_FILE_PDF_O);
ImGui::Text(ICON_FILE_WORD_O);
ImGui::Text(ICON_FILE_EXCEL_O);
ImGui::Text(ICON_FILE_POWERPOINT_O);
ImGui::Text(ICON_FILE_IMAGE_O);
ImGui::Text(ICON_FILE_ARCHIVE_O);
ImGui::Text(ICON_FILE_AUDIO_O);
ImGui::Text(ICON_FILE_VIDEO_O);
ImGui::Text(ICON_FILE_CODE_O);
ImGui::Text(ICON_VINE);
ImGui::Text(ICON_CODEPEN);
ImGui::Text(ICON_JSFIDDLE);
ImGui::Text(ICON_LIFE_RING);
ImGui::Text(ICON_CIRCLE_O_NOTCH);
ImGui::Text(ICON_REBEL);
ImGui::Text(ICON_EMPIRE);
ImGui::Text(ICON_GIT_SQUARE);
ImGui::Text(ICON_GIT);
ImGui::Text(ICON_HACKER_NEWS);
ImGui::Text(ICON_TENCENT_WEIBO);
ImGui::Text(ICON_QQ);
ImGui::Text(ICON_WEIXIN);
ImGui::Text(ICON_PAPER_PLANE);
ImGui::Text(ICON_PAPER_PLANE_O);
ImGui::Text(ICON_HISTORY);
ImGui::Text(ICON_CIRCLE_THIN);
ImGui::Text(ICON_HEADER);
ImGui::Text(ICON_PARAGRAPH);
ImGui::Text(ICON_SLIDERS);
ImGui::Text(ICON_SHARE_ALT);
ImGui::Text(ICON_SHARE_ALT_SQUARE);
ImGui::Text(ICON_BOMB);
ImGui::Text(ICON_FUTBOL_O);
ImGui::Text(ICON_TTY);
ImGui::Text(ICON_BINOCULARS);
ImGui::Text(ICON_PLUG);
ImGui::Text(ICON_SLIDESHARE);
ImGui::Text(ICON_TWITCH);
ImGui::Text(ICON_YELP);
ImGui::Text(ICON_NEWSPAPER_O);
ImGui::Text(ICON_WIFI);
ImGui::Text(ICON_CALCULATOR);
ImGui::Text(ICON_PAYPAL);
ImGui::Text(ICON_GOOGLE_WALLET);
ImGui::Text(ICON_CC_VISA);
ImGui::Text(ICON_CC_MASTERCARD);
ImGui::Text(ICON_CC_DISCOVER);
ImGui::Text(ICON_CC_AMEX);
ImGui::Text(ICON_CC_PAYPAL);
ImGui::Text(ICON_CC_STRIPE);
ImGui::Text(ICON_BELL_SLASH);
ImGui::Text(ICON_BELL_SLASH_O);
ImGui::Text(ICON_TRASH);
ImGui::Text(ICON_COPYRIGHT);
ImGui::Text(ICON_AT);
ImGui::Text(ICON_EYEDROPPER);
ImGui::Text(ICON_PAINT_BRUSH);
ImGui::Text(ICON_BIRTHDAY_CAKE);
ImGui::Text(ICON_AREA_CHART);
ImGui::Text(ICON_PIE_CHART);
ImGui::Text(ICON_LINE_CHART);
ImGui::Text(ICON_LASTFM);
ImGui::Text(ICON_LASTFM_SQUARE);
ImGui::Text(ICON_TOGGLE_OFF);
ImGui::Text(ICON_TOGGLE_ON);
ImGui::Text(ICON_BICYCLE);
ImGui::Text(ICON_BUS);
ImGui::Text(ICON_IOXHOST);
ImGui::Text(ICON_ANGELLIST);
ImGui::Text(ICON_CC);
ImGui::Text(ICON_ILS);
ImGui::Text(ICON_MEANPATH);
ImGui::Text(ICON_BUYSELLADS);
ImGui::Text(ICON_CONNECTDEVELOP);
ImGui::Text(ICON_DASHCUBE);
ImGui::Text(ICON_FORUMBEE);
ImGui::Text(ICON_LEANPUB);
ImGui::Text(ICON_SELLSY);
ImGui::Text(ICON_SHIRTSINBULK);
ImGui::Text(ICON_SIMPLYBUILT);
ImGui::Text(ICON_SKYATLAS);
ImGui::Text(ICON_CART_PLUS);
ImGui::Text(ICON_CART_ARROW_DOWN);
ImGui::Text(ICON_DIAMOND);
ImGui::Text(ICON_SHIP);
ImGui::Text(ICON_USER_SECRET);
ImGui::Text(ICON_MOTORCYCLE);
ImGui::Text(ICON_STREET_VIEW);
ImGui::Text(ICON_HEARTBEAT);
ImGui::Text(ICON_VENUS);
ImGui::Text(ICON_MARS);
ImGui::Text(ICON_MERCURY);
ImGui::Text(ICON_TRANSGENDER);
ImGui::Text(ICON_TRANSGENDER_ALT);
ImGui::Text(ICON_VENUS_DOUBLE);
ImGui::Text(ICON_MARS_DOUBLE);
ImGui::Text(ICON_VENUS_MARS);
ImGui::Text(ICON_MARS_STROKE);
ImGui::Text(ICON_MARS_STROKE_V);
ImGui::Text(ICON_MARS_STROKE_H);
ImGui::Text(ICON_NEUTER);
ImGui::Text(ICON_GENDERLESS);
ImGui::Text(ICON_FACEBOOK_OFFICIAL);
ImGui::Text(ICON_PINTEREST_P);
ImGui::Text(ICON_WHATSAPP);
ImGui::Text(ICON_SERVER);
ImGui::Text(ICON_USER_PLUS);
ImGui::Text(ICON_USER_TIMES);
ImGui::Text(ICON_BED);
ImGui::Text(ICON_VIACOIN);
ImGui::Text(ICON_TRAIN);
ImGui::Text(ICON_SUBWAY);
ImGui::Text(ICON_MEDIUM);
ImGui::Text(ICON_MEDIUM_SQUARE);
ImGui::Text(ICON_Y_COMBINATOR);
ImGui::Text(ICON_OPTIN_MONSTER);
ImGui::Text(ICON_OPENCART);
ImGui::Text(ICON_EXPEDITEDSSL);
ImGui::Text(ICON_BATTERY_FULL);
ImGui::Text(ICON_BATTERY_THREE_QUARTERS);
ImGui::Text(ICON_BATTERY_HALF);
ImGui::Text(ICON_BATTERY_QUARTER);
ImGui::Text(ICON_BATTERY_EMPTY);
ImGui::Text(ICON_MOUSE_POINTER);
ImGui::Text(ICON_I_CURSOR);
ImGui::Text(ICON_OBJECT_GROUP);
ImGui::Text(ICON_OBJECT_UNGROUP);
ImGui::Text(ICON_STICKY_NOTE);
ImGui::Text(ICON_STICKY_NOTE_O);
ImGui::Text(ICON_CC_JCB);
ImGui::Text(ICON_CC_DINERS_CLUB);
ImGui::Text(ICON_CLONE);
ImGui::Text(ICON_BALANCE_SCALE);
ImGui::Text(ICON_HOURGLASS_O);
ImGui::Text(ICON_HOURGLASS_START);
ImGui::Text(ICON_HOURGLASS_HALF);
ImGui::Text(ICON_HOURGLASS_END);
ImGui::Text(ICON_HOURGLASS);
ImGui::Text(ICON_HAND_ROCK_O);
ImGui::Text(ICON_HAND_PAPER_O);
ImGui::Text(ICON_HAND_SCISSORS_O);
ImGui::Text(ICON_HAND_LIZARD_O);
ImGui::Text(ICON_HAND_SPOCK_O);
ImGui::Text(ICON_HAND_POINTER_O);
ImGui::Text(ICON_HAND_PEACE_O);
ImGui::Text(ICON_TRADEMARK);
ImGui::Text(ICON_REGISTERED);
ImGui::Text(ICON_CREATIVE_COMMONS);
ImGui::Text(ICON_GG);
ImGui::Text(ICON_GG_CIRCLE);
ImGui::Text(ICON_TRIPADVISOR);
ImGui::Text(ICON_ODNOKLASSNIKI);
ImGui::Text(ICON_ODNOKLASSNIKI_SQUARE);
ImGui::Text(ICON_GET_POCKET);
ImGui::Text(ICON_WIKIPEDIA_W);
ImGui::Text(ICON_SAFARI);
ImGui::Text(ICON_CHROME);
ImGui::Text(ICON_FIREFOX);
ImGui::Text(ICON_OPERA);
ImGui::Text(ICON_INTERNET_EXPLORER);
ImGui::Text(ICON_TELEVISION);
ImGui::Text(ICON_CONTAO);
ImGui::Text(ICON_500PX);
ImGui::Text(ICON_AMAZON);
ImGui::Text(ICON_CALENDAR_PLUS_O);
ImGui::Text(ICON_CALENDAR_MINUS_O);
ImGui::Text(ICON_CALENDAR_TIMES_O);
ImGui::Text(ICON_CALENDAR_CHECK_O);
ImGui::Text(ICON_INDUSTRY);
ImGui::Text(ICON_MAP_PIN);
ImGui::Text(ICON_MAP_SIGNS);
ImGui::Text(ICON_MAP_O);
ImGui::Text(ICON_MAP);
ImGui::Text(ICON_COMMENTING);
ImGui::Text(ICON_COMMENTING_O);
ImGui::Text(ICON_HOUZZ);
ImGui::Text(ICON_VIMEO);
ImGui::Text(ICON_BLACK_TIE);
ImGui::Text(ICON_FONTICONS);
ImGui::Text(ICON_REDDIT_ALIEN);
ImGui::Text(ICON_EDGE);
ImGui::Text(ICON_CREDIT_CARD_ALT);
ImGui::Text(ICON_CODIEPIE);
ImGui::Text(ICON_MODX);
ImGui::Text(ICON_FORT_AWESOME);
ImGui::Text(ICON_USB);
ImGui::Text(ICON_PRODUCT_HUNT);
ImGui::Text(ICON_MIXCLOUD);
ImGui::Text(ICON_SCRIBD);
ImGui::Text(ICON_PAUSE_CIRCLE);
ImGui::Text(ICON_PAUSE_CIRCLE_O);
ImGui::Text(ICON_STOP_CIRCLE);
ImGui::Text(ICON_STOP_CIRCLE_O);
ImGui::Text(ICON_SHOPPING_BAG);
ImGui::Text(ICON_SHOPPING_BASKET);
ImGui::Text(ICON_HASHTAG);
ImGui::Text(ICON_BLUETOOTH);
ImGui::Text(ICON_BLUETOOTH_B);
ImGui::Text(ICON_PERCENT);
ImGui::Text(ICON_GITLAB);
ImGui::Text(ICON_WPBEGINNER);
ImGui::Text(ICON_WPFORMS);
ImGui::Text(ICON_ENVIRA);
ImGui::Text(ICON_UNIVERSAL_ACCESS);
ImGui::Text(ICON_WHEELCHAIR_ALT);
ImGui::Text(ICON_QUESTION_CIRCLE_O);
ImGui::Text(ICON_BLIND);
ImGui::Text(ICON_AUDIO_DESCRIPTION);
ImGui::Text(ICON_VOLUME_CONTROL_PHONE);
ImGui::Text(ICON_BRAILLE);
ImGui::Text(ICON_ASSISTIVE_LISTENING_SYSTEMS);
ImGui::Text(ICON_AMERICAN_SIGN_LANGUAGE_INTERPRETING);
ImGui::Text(ICON_DEAF);
ImGui::Text(ICON_GLIDE);
ImGui::Text(ICON_GLIDE_G);
ImGui::Text(ICON_SIGN_LANGUAGE);
ImGui::Text(ICON_LOW_VISION);
ImGui::Text(ICON_VIADEO);
ImGui::Text(ICON_VIADEO_SQUARE);
ImGui::Text(ICON_SNAPCHAT);
ImGui::Text(ICON_SNAPCHAT_GHOST);
ImGui::Text(ICON_SNAPCHAT_SQUARE);
ImGui::Text(ICON_FIRST_ORDER);
ImGui::Text(ICON_YOAST);
ImGui::Text(ICON_THEMEISLE);
ImGui::Text(ICON_GOOGLE_PLUS_OFFICIAL);
ImGui::Text(ICON_FONT_AWESOME);
ImGui::Text(ICON_HANDSHAKE_O);
ImGui::Text(ICON_ENVELOPE_OPEN);
ImGui::Text(ICON_ENVELOPE_OPEN_O);
ImGui::Text(ICON_LINODE);
ImGui::Text(ICON_ADDRESS_BOOK);
ImGui::Text(ICON_ADDRESS_BOOK_O);
ImGui::Text(ICON_ADDRESS_CARD);
ImGui::Text(ICON_ADDRESS_CARD_O);
ImGui::Text(ICON_USER_CIRCLE);
ImGui::Text(ICON_USER_CIRCLE_O);
ImGui::Text(ICON_USER_O);
ImGui::Text(ICON_ID_BADGE);
ImGui::Text(ICON_ID_CARD);
ImGui::Text(ICON_ID_CARD_O);
ImGui::Text(ICON_QUORA);
ImGui::Text(ICON_FREE_CODE_CAMP);
ImGui::Text(ICON_TELEGRAM);
ImGui::Text(ICON_THERMOMETER_FULL);
ImGui::Text(ICON_THERMOMETER_THREE_QUARTERS);
ImGui::Text(ICON_THERMOMETER_HALF);
ImGui::Text(ICON_THERMOMETER_QUARTER);
ImGui::Text(ICON_THERMOMETER_EMPTY);
ImGui::Text(ICON_SHOWER);
ImGui::Text(ICON_BATH);
ImGui::Text(ICON_PODCAST);
ImGui::Text(ICON_WINDOW_MAXIMIZE);
ImGui::Text(ICON_WINDOW_MINIMIZE);
ImGui::Text(ICON_WINDOW_RESTORE);
ImGui::Text(ICON_WINDOW_CLOSE);
ImGui::Text(ICON_WINDOW_CLOSE_O);
ImGui::Text(ICON_BANDCAMP);
ImGui::Text(ICON_GRAV);
ImGui::Text(ICON_ETSY);
ImGui::Text(ICON_IMDB);
ImGui::Text(ICON_RAVELRY);
ImGui::Text(ICON_EERCAST);
ImGui::Text(ICON_MICROCHIP);
ImGui::Text(ICON_SNOWFLAKE_O);
ImGui::Text(ICON_SUPERPOWERS);
ImGui::Text(ICON_WPEXPLORER);
ImGui::Text(ICON_MEETUP);
ImGui::Text(ICON_MASTODON);
ImGui::Text(ICON_MASTODON_ALT);
ImGui::Text(ICON_FORK_AWESOME);
ImGui::Text(ICON_PEERTUBE);
ImGui::Text(ICON_DIASPORA);
ImGui::Text(ICON_FRIENDICA);
ImGui::Text(ICON_GNU_SOCIAL);
ImGui::Text(ICON_LIBERAPAY_SQUARE);
ImGui::Text(ICON_LIBERAPAY);
ImGui::Text(ICON_SCUTTLEBUTT);
ImGui::Text(ICON_HUBZILLA);
ImGui::Text(ICON_SOCIAL_HOME);
ImGui::Text(ICON_ARTSTATION);
ImGui::Text(ICON_DISCORD);
ImGui::Text(ICON_DISCORD_ALT);
ImGui::Text(ICON_PATREON);
ImGui::Text(ICON_SNOWDRIFT);
ImGui::Text(ICON_ACTIVITYPUB);
ImGui::Text(ICON_ETHEREUM);
ImGui::Text(ICON_KEYBASE);
ImGui::Text(ICON_SHAARLI);
ImGui::Text(ICON_SHAARLI_O);
ImGui::Text(ICON_KEY_MODERN);
ImGui::Text(ICON_XMPP);
ImGui::Text(ICON_ARCHIVE_ORG);
ImGui::Text(ICON_FREEDOMBOX);
ImGui::Text(ICON_FACEBOOK_MESSENGER);
ImGui::Text(ICON_DEBIAN);
ImGui::Text(ICON_MASTODON_SQUARE);
ImGui::Text(ICON_TIPEEE);
ImGui::Text(ICON_REACT);
ImGui::Text(ICON_DOGMAZIC);
ImGui::Text(ICON_NEXTCLOUD);
ImGui::Text(ICON_NEXTCLOUD_SQUARE);

ImGui::End();
#endif
