#include "DojoGui.hpp"
#include <hw/naomi/naomi_cart.h>
#include <regex>
namespace fs = ghc::filesystem;

void DojoGui::gui_display_bios_rom_warning(float scaling)
{
	std::string current_bios;
	if (settings.platform.system == DC_PLATFORM_NAOMI)
		current_bios = "naomi.zip";
	else if (settings.platform.system == DC_PLATFORM_ATOMISWAVE)
		current_bios = "awbios.zip";

	std::string file_path = settings.content.path;
	std::string current_filename = file_path.substr(file_path.find_last_of("/\\") + 1);

	std::string designation, start_msg, file_d;

	if (!bios_json_match && !current_json_match)
	{
		designation = "BIOS & ROM";
		start_msg = current_bios + " & " + current_filename + " do";
		file_d = "files";
	}
	else if (!bios_json_match)
	{
		designation = "BIOS";
		start_msg = current_bios + " does";
		file_d = "file";
	}
	else if (!current_json_match)
	{
		designation = "ROM";
		start_msg = current_filename + " does";
		file_d = "file";
	}

	std::string popup_title = designation + " Mismatch";

	ImGui::OpenPopup(popup_title.data());
	if (ImGui::BeginPopupModal(popup_title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string msg = start_msg + " not match the checksum of community-recommended " + file_d + ".\nPlease find a new " + designation + " and try again.";
		ImGui::TextColored(ImVec4(128, 0, 0, 1), "WARNING");
		ImGui::TextColored(ImVec4(128, 128, 0, 1), "%s", msg.data());
		std::string msg_2 = "Having a different " + designation + " than your opponent may result in desyncs.\nProceed at your own risk.";
		ImGui::TextUnformatted(msg_2.data());

		if (ImGui::Button("Continue"))
		{
			if (config::TestGame)
			{
				if (strlen(settings.content.path.data()) > 0)
					gui_start_game(settings.content.path);
				else
					gui_state = GuiState::Main;
			}
			else if (config::GGPOEnable)
			{
				if (config::EnableMatchCode)
				{
					// use dojo session for match codes & initial handshake
					dojo.StartDojoSession();

					if (config::ActAsServer)
						gui_open_host_wait();
					else
						gui_open_guest_wait();
				}
				else
				{
					gui_open_ggpo_join();
				}
			}
			else if (config::DojoActAsServer)
			{
				dojo.host_status = 1;
				if (config::EnableLobby)
					dojo.remaining_spectators = config::Transmitting ? 1 : 0;
				else
					dojo.remaining_spectators = 0;
				gui_open_host_wait();
			}
			else
			{
				gui_open_guest_wait();
			}
		}

		/*
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();

			// Exit to main menu
			gui_state = GuiState::Main;
			game_started = false;
			settings.content.path = "";
			dc_reset(true);
		}
		*/

		ImGui::EndPopup();
	}

}

void DojoGui::gui_open_host_delay()
{
	gui_state = GuiState::HostDelay;
}

void DojoGui::gui_display_host_wait(float scaling)
{
	//emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##host_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("Waiting for opponent to connect...");

	if (config::EnableMatchCode && !config::MatchCode.get().empty())
	{
		ImGui::Text("Match Code: %s", config::MatchCode.get().data());
		ImGui::SameLine();
		ShowHelpMarker("Match Codes not working?\nTry switching to Direct IP in the settings.");
#ifndef __ANDROID__
		if (ImGui::Button("Copy Match Code"))
		{
			SDL_SetClipboardText(config::MatchCode.get().data());
		}
#endif
	}

	/*
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
	{
		ImGui::CloseCurrentPopup();

		// Exit to main menu
		gui_state = GuiState::Main;
		game_started = false;
		settings.content.path = "";
		dc_reset(true);
	}
	*/

	if (config::GGPOEnable)
	{
		if (!config::NetworkServer.get().empty())
		{
			gui_open_ggpo_join();
		}
	}
	else
	{
		if (!dojo.OpponentIP.empty())
		{
			dojo.host_status = 2;
			dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

			gui_state = GuiState::Closed;
			gui_open_host_delay();
		}

		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			if (config::EnableLobby)
				ImGui::Text("This match will be spectated.");
		}
	}

	ImGui::End();
}

void DojoGui::gui_display_guest_wait(float scaling)
{
	//emu.term();

	if (!config::GGPOEnable)
		dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##guest_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (!dojo.client.name_acknowledged)
	{
		if ((!config::GGPOEnable && config::EnableMatchCode && config::MatchCode.get().empty()) ||
			(config::GGPOEnable && config::EnableMatchCode && config::NetworkServer.get().empty()))
		{
			ImGui::OpenPopup("Match Code");
			if (ImGui::BeginPopupModal("Match Code", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::Text("Enter Match Code generated by host.");

				static char mc[128] = "";
				ImGui::InputTextWithHint("", "ABC123", mc, IM_ARRAYSIZE(mc), ImGuiInputTextFlags_CharsUppercase);

#ifndef __ANDROID__
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(mc, pasted_txt, strlen(pasted_txt));
				}
#endif
				if (ImGui::Button("Start Session"))
				{
					dojo.MatchCode = std::string(mc, strlen(mc));
					ImGui::CloseCurrentPopup();
				}

				/*
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);
				}
				*/

				ImGui::EndPopup();
			}
		}
		else if (!config::GGPOEnable && config::DojoServerIP.get().empty())
		{
   			ImGui::OpenPopup("Connect to Host Server");
   			if (ImGui::BeginPopupModal("Connect to Host Server", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
   			{
   				ImGui::Text("Enter Host Server Details");

   				static char si[128] = "";
   				ImGui::InputTextWithHint("IP", "0.0.0.0", si, IM_ARRAYSIZE(si));
#ifndef __ANDROID__
				ImGui::SameLine();
				if (ImGui::Button("Paste"))
				{
					char* pasted_txt = SDL_GetClipboardText();
					memcpy(si, pasted_txt, strlen(pasted_txt));
				}
#endif
   				static char sp[128] = "6000";
   				ImGui::InputTextWithHint("Port", "6000", sp, IM_ARRAYSIZE(sp));

   				if (ImGui::Button("Start Session"))
   				{
					config::DojoServerIP = std::string(si, strlen(si));
					config::DojoServerPort = std::string(sp, strlen(sp));
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
					cfgSaveStr("dojo", "ServerPort", config::DojoServerPort.get().c_str());

					dojo.client.SetHost(config::DojoServerIP, atoi(config::DojoServerPort.get().data()));

   					ImGui::CloseCurrentPopup();
   				}

				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					ImGui::CloseCurrentPopup();

					// Exit to main menu
					gui_state = GuiState::Main;
					game_started = false;
					settings.content.path = "";
					dc_reset(true);

					config::DojoServerIP = "";
					cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.get().c_str());
				}

   				ImGui::EndPopup();
   			}
		}

		if (!config::GGPOEnable && !config::DojoServerIP.get().empty())
		{
			ImGui::Text("Connecting to host...");

			/*
			ImGui::SameLine();
			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();

				// Exit to main menu
				gui_state = GuiState::Main;
				game_started = false;
				settings.content.path = "";
				dc_reset(true);

				config::DojoServerIP = "";
				cfgSaveStr("dojo", "ServerIP", config::DojoServerIP.data().c_str());
			}
			*/

			dojo.client.SendPlayerName();
		}
	}
	else
	{
		ImGui::Text("Waiting for host to select delay...");
	}

	if (config::GGPOEnable && !config::NetworkServer.get().empty())
		gui_open_ggpo_join();

	if (dojo.session_started)
	{
		gui_state = GuiState::Closed;
		dojo.resume();
	}

	ImGui::End();
}


void DojoGui::gui_display_stream_wait(float scaling)
{
	//emu.term();

	if(buffer_captured)
		gui_state = GuiState::Loading;

	if (!config::GGPOEnable)
		dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##stream_wait", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (config::Receiving)
	{
		if (dojo.maple_inputs.size() == 0)
			ImGui::Text("WAITING FOR MATCH STREAM TO BEGIN...");
		else
		{
			float progress = (float)dojo.maple_inputs.size() / (float)config::RxFrameBuffer.get();

			ImGui::Text("Buffering Match Stream...");
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));
			ImGui::ProgressBar(progress, ImVec2(-1, 20.f * scaling), "");
			ImGui::PopStyleColor();

			ImGui::Text("%d / %d Frames", dojo.maple_inputs.size(), config::RxFrameBuffer.get());
		}
	}

	ImGui::End();

	if (dojo.maple_inputs.size() > config::RxFrameBuffer.get())
	{
		buffer_captured = true;
	}
}

std::string longestCommonPrefix(std::vector<std::string>& str) {
	int n = str.size();
	if(n==0) return "";

	std::string ans  = "";
	sort(begin(str), end(str));
	std::string a = str[0];
	std::string b = str[n-1];

	for(int i=0; i<a.size(); i++){
		if(a[i]==b[i]){
			ans = ans + a[i];
		}
		else{
			break;
		}
	}

	return ans;
}

void DojoGui::gui_display_ggpo_join(float scaling)
{
	std::string title;
	if (config::EnableMatchCode)
	{
		title = "Select GGPO Frame Delay";
	}
	else
	{
		if (config::NumPlayers > 2)
			title = "Connect to GGPO Opponents";
		else
			title = "Connect to GGPO Opponent";
	}
	ImGui::OpenPopup(title.data());
	if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		static char si[128] = "";
		static char sis[4][128] = { "", "", "", "" };
		std::string detect_address = "";

		if (config::EnableMatchCode)
		{
			detect_address = config::NetworkServer.get();

			// if both player names are defaults, hide names
			if (!config::GGPOEnable && config::NumPlayers == 2)
			{
				if (!(config::PlayerName.get().compare("Player") == 0 &&
					config::PlayerName.get().compare(config::OpponentName.get()) == 0))
				{
					if (config::ActAsServer)
						ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
					else
						ImGui::Text("%s vs %s", config::OpponentName.get().c_str(), config::PlayerName.get().c_str());
				}
			}
		}
		else if (!dojo.commandLineStart && !dojo.lobby_host_screen)
		{
			if ((!config::ManualPlayerAssign && config::NetworkServer.get().empty()) ||
				(config::ManualPlayerAssign && config::NetworkP0Server.get().empty()))
			{

				if (config::ShowPublicIP)
				{
					static char external_ip[128] = "";
					if (current_public_ip.empty())
					{
						current_public_ip = dojo.GetExternalIP();
						const char* external = current_public_ip.data();
						memcpy(external_ip, external, strlen(external));
					}

					ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(255, 255, 0, 1));
					ImGui::Text("Your Public IP: ");
					ImGui::SameLine();
					ImGui::TextDisabled("%s", external_ip);
					ImGui::PopStyleColor();
					ImGui::SameLine();
					ShowHelpMarker("This is your public IP to share with your opponent.\nFor Virtual LANs, refer to your software.");
#ifndef __ANDROID__
					ImGui::SameLine();
					if (ImGui::Button("Copy"))
					{
						SDL_SetClipboardText(current_public_ip.data());
					}
#endif
				}

				std::string num_slider_desc = "Adjust number of players.";
				if (!config::ActAsServer && config::NumPlayers > 2)
					num_slider_desc = num_slider_desc + " . Select your player below.";
				OptionSlider("# Players", config::NumPlayers, 2, 4, num_slider_desc.data());

				if (config::NumPlayers == 2)
				{
					config::ManualPlayerAssign = false;
					std::string player_hint = config::ActAsServer ? "P2" : "P1";
					ImGui::Text("%s", player_hint.c_str());
					ImGui::SameLine();
					ImGui::InputTextWithHint("", "0.0.0.0", si, IM_ARRAYSIZE(si));
					detect_address = std::string(si);
#ifndef __ANDROID__
					ImGui::SameLine();
					if (ImGui::Button("Paste"))
					{
						char* pasted_txt = SDL_GetClipboardText();
						memcpy(si, pasted_txt, strlen(pasted_txt));
					}
#endif
				}
				else
				{
					config::ManualPlayerAssign = true;
					for (int i = 0; i < config::NumPlayers; i++)
					{
						if (config::ActAsServer && i == 0)
							continue;

						std::string player_hint = "P" + std::to_string(i + 1);
						if (!config::ActAsServer)
						{
							if (i == 0)
							{
								ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
								ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
							}
							OptionRadioButton(player_hint.c_str(), config::PlayerNum, i, nullptr);
							if (i == 0)
							{
								ImGui::PopItemFlag();
								ImGui::PopStyleVar();
							}
						}
						else
						{
							ImGui::Text("%s", player_hint.c_str());
						}

						if (i == config::PlayerNum)
							continue;

						ImGui::SameLine();

						std::string player_label = "##" + player_hint;
						ImGui::PushItemWidth(240);
						ImGui::InputTextWithHint(player_label.c_str(), "0.0.0.0:19713", sis[i], IM_ARRAYSIZE(sis[i]));
						ImGui::PopItemWidth();
#ifndef __ANDROID__
						ImGui::SameLine();
						std::string paste_label = "Paste##" + player_hint;
						if (ImGui::Button(paste_label.c_str()))
						{
							char* pasted_txt = SDL_GetClipboardText();
							memcpy(&sis[i], pasted_txt, strlen(pasted_txt));
						}
#endif
					}
				}
			}
		}

		ImGui::PushItemWidth(325);
		ImGui::SliderInt("##Delay", (int*)&dojo.current_delay, 0, 20);
		ImGui::SameLine();
		ImGui::Text("Delay");
		ImGui::PopItemWidth();

		if (config::EnableMatchCode && dojo.host_status < 1)
		{
			if (ImGui::Button("Detect Delay"))
				dojo.OpponentPing = dojo.DetectGGPODelay(detect_address.data());

			if (dojo.OpponentPing > 0)
			{
				ImGui::SameLine();
				ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
			}
		}

#ifndef __ANDROID__
		if (!dojo.commandLineStart && !(dojo.host_status == 2 || dojo.host_status == 5) && config::NumPlayers > 2)
		{
			if (ImGui::Button("Copy as Session Code"))
			{
				std::string ex_code = "|";

				std::vector<std::string> local_ips;
				std::vector<std::string> ip_prefixes;

				for (int i = 0; i < config::NumPlayers.get(); i++)
				{
					if (config::ActAsServer && i == 0)
						continue;

					std::string ip_str = std::string(sis[i]);
					ip_prefixes.push_back(ip_str);
				}

				std::string lcp = longestCommonPrefix(ip_prefixes);
				lcp = lcp.substr(0, lcp.find_last_of("."));

				auto iadd = dojo.GetLocalInterfaces();
				for (auto ia : iadd)
				{
					auto ip_str = ia.to_string();
					auto dot_found = ip_str.find(".");
					if (dot_found == std::string::npos)
						continue;

					DEBUG_LOG(NETWORK, "IP: %s\n", ip_str.data());
					local_ips.push_back(ip_str);
				}

				std::string assigned_ip = "";

				auto substring = lcp;
				const auto iter = std::find_if(local_ips.begin(), local_ips.end(),
					[&substring](std::string str) {
						return str.find(substring) == 0;
					}
				);

				if (iter != local_ips.end())
				{
					assigned_ip = *iter;
				}

				if (assigned_ip == "")
					assigned_ip = dojo.GetExternalIP();

				assigned_ip = assigned_ip + ":" + std::to_string(config::GGPOPort.get());

				memcpy(sis[0], assigned_ip.data(), assigned_ip.size());
				NOTICE_LOG(NETWORK, "ASSIGNED: %s\n", assigned_ip.data());

				for (int i = 0; i < config::NumPlayers.get(); i++)
				{
					std::string player_entry = std::string(sis[i], strlen(sis[i]));
					if (player_entry.find(':') == std::string::npos)
					{
						int player_port = config::GGPOP0Port.get();
						if (i == 1)
							player_port = config::GGPOP1Port.get();
						else if (i == 2)
							player_port = config::GGPOP2Port.get();
						else if (i == 3)
							player_port = config::GGPOP3Port.get();

						player_entry = player_entry + ":" + std::to_string(player_port);
					}
					ex_code = ex_code + player_entry + "|";
				}

				std::string encoded_ex = base64_encode(reinterpret_cast<const unsigned char*>(ex_code.data()), ex_code.length());
				SDL_SetClipboardText(encoded_ex.data());
			}

			ImGui::SameLine();
			if (ImGui::Button("Paste from Session Code"))
			{
				char* pasted_txt = SDL_GetClipboardText();
				auto pasted_txt_str = std::string(pasted_txt);

				// validate base64
				std::string base64_regex = "^([A-Za-z0-9+/]{4})*([A-Za-z0-9+/]{3}=|[A-Za-z0-9+/]{2}==)?$";
				std::regex rx(base64_regex);
				if (pasted_txt_str.size() % 4 == 0 && std::regex_match(pasted_txt_str.data(), rx))
				{
					NOTICE_LOG(NETWORK, "Pasted Session Code: %s", pasted_txt);
					std::string decoded_paste = base64_decode(std::string(pasted_txt));

					// validate inclusion of delimiter
					if (decoded_paste.find("|") != std::string::npos)
					{
						auto targets = stringfix::split("|", decoded_paste);
						for (int i = 0; i < targets.size(); i++)
						{
							memcpy(sis[i], targets[i].data(), targets[i].size());
						}
					}
				}
			}

			ImGui::SameLine();
			ShowHelpMarker("Populate address entries from Session Code for easy sharing among opponents.");
		}
#endif

		if (ImGui::Button("Start Session"))
		{
			if (dojo.lobby_launch && config::ActAsServer)
				dojo.host_status = 3;

			if (dojo.current_delay != config::GGPODelay.get())
				config::GGPODelay.set(dojo.current_delay);

			if (!config::EnableMatchCode && !dojo.lobby_launch && !dojo.commandLineStart)
			{
				if (!dojo.commandLineStart)
				{
					if (config::NumPlayers > 2)
					{
						std::string servers[4];
						int ports[4];

						for (int i = 0; i < config::NumPlayers; i++)
						{
							if (strlen(sis[i]) > 0)
							{
								auto target = stringfix::split(":", std::string(sis[i]));
								servers[i] = target[0];
								if (target.size() > 1)
									ports[i] = std::stoi(target[1]);
								else
								{
									if (i == config::PlayerNum)
										ports[i] = config::GGPOPort.get();
									else
										ports[i] = 0;
								}
							}
							else
							{
								if (i == 0)
									servers[i] = config::NetworkP0Server;
								else if (i == 1)
									servers[i] = config::NetworkP1Server;
								else if (i == 2)
									servers[i] = config::NetworkP2Server;
								else if (i == 3)
									servers[i] = config::NetworkP3Server;
							}

							if (i == 0)
							{
								config::NetworkP0Server.set(servers[i]);

								if (ports[i] > 0)
									config::GGPOP0Port.set(ports[i]);
							}
							else if (i == 1)
							{
								config::NetworkP1Server.set(servers[i]);

								if (ports[i] > 0)
									config::GGPOP1Port.set(ports[i]);
							}
							else if (i == 2)
							{
								config::NetworkP2Server.set(servers[i]);

								if (ports[i] > 0)
									config::GGPOP2Port.set(ports[i]);
							}
							else if (i == 3)
							{
								config::NetworkP3Server.set(servers[i]);

								if (ports[i] > 0)
									config::GGPOP3Port.set(ports[i]);
							}

						}
					}

					config::NetworkServer.set(std::string(si, strlen(si)));
					//cfgSaveStr("network", "server", config::NetworkServer.get());
				}

				config::DojoEnable = false;
			}
			ggpo_join_screen = false;
			ImGui::CloseCurrentPopup();
			start_ggpo();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			config::GGPOEnable = false;
			ggpo_join_screen = false;
			ImGui::CloseCurrentPopup();

			// Exit to main menu
			gui_state = GuiState::Main;
			game_started = false;
			settings.content.path = "";
			dc_reset(true);

			if (dojo.host_status > 0)
			{
				dojo.host_status = 0;
				dojo.lobby_host_screen = false;
				settings.content.path = "";

				dojo.beacon_active = false;
				dojo.lobby_launch = false;

				dojo_gui.item_current_idx = 0;
			}

			config::NetworkServer.set("");
			config::NetworkP0Server.set("");
			config::NetworkP1Server.set("");
			config::NetworkP2Server.set("");
			config::NetworkP3Server.set("");
		}

		float comboWidth = ImGui::CalcTextSize("Button Check").x + ImGui::GetStyle().ItemSpacing.x + ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 4;
		ImGui::SameLine(0, 128.0f + ImGui::CalcTextSize("IP").x + ImGui::CalcTextSize("Paste").x - ImGui::CalcTextSize("Start Session").x);

		if (ImGui::Button("Button Check"))
		{
			ggpo_join_screen = true;
			gui_state = GuiState::ButtonCheck;
		}

		ImGui::EndPopup();
	}
}

void DojoGui::gui_display_disconnected( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##disconnected", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (dojo.client.opponent_disconnected)
		ImGui::Text("Opponent disconnected.");
	else
		ImGui::Text("Disconnected.");

	if (ImGui::Button("Exit Game"))
	{
		gui_stop_game();
	}

	ImGui::End();

	//config::AutoSkipFrame = 1;

	dojo.CleanUp();

	error_popup();
}

void DojoGui::gui_display_end_replay( float scaling)
{
	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_replay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of replay.");

	if (ImGui::Button("Exit Game"))
	{
		gui_stop_game();
	}

	ImGui::End();
}

void DojoGui::gui_display_end_spectate( float scaling)

{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##end_spectate", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Text("End of spectated match.");

	ImGui::End();
}

void DojoGui::gui_display_host_delay( float scaling)

{
	//emu.term();

	dojo.pause();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(360 * scaling, 0));

	ImGui::Begin("##host_delay", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	// if both player names are defaults, hide names
	if (!(config::PlayerName.get().compare("Player") == 0 &&
		config::PlayerName.get().compare(config::OpponentName.get()) == 0))
	{
		ImGui::Text("%s vs %s", config::PlayerName.get().c_str(), config::OpponentName.get().c_str());
	}

	ImGui::SliderInt("", (int*)&config::Delay.get(), 1, 10);
	ImGui::SameLine();
	ImGui::Text("Set Delay");

	if (ImGui::Button("Detect Delay"))
		dojo.OpponentPing = dojo.DetectDelay(dojo.OpponentIP.data());

	if (dojo.OpponentPing > 0)
	{
		ImGui::SameLine();
		ImGui::Text("Current Ping: %d ms", dojo.OpponentPing);
	}

	if (ImGui::Button("Start Game"))
	{
		dojo.PlayMatch = false;
		if (config::Transmitting &&
			dojo.remaining_spectators == 0)
		{
			dojo.StartTransmitterThread();
		}

		dojo.isMatchStarted = true;
		dojo.StartSession(config::Delay.get(),
			config::PacketsPerFrame,
			config::NumBackFrames);
		dojo.resume();

		gui_state = GuiState::Closed;
	}

	SaveSettings();

	ImGui::End();
}

void DojoGui::gui_display_test_game( float scaling)
{
	emu.term();

	ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(330 * scaling, 0));

	ImGui::Begin("##test_game", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	ImGui::Columns(2, "buttons", false);
	if (ImGui::Button("Settings", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Settings;
	}
	ImGui::NextColumn();
	if (ImGui::Button("Start Game", ImVec2(150 * scaling, 50 * scaling)))
	{
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

	}

	ImGui::NextColumn();
	if (ImGui::Button("Button Check", ScaledVec2(150, 50)) && !settings.network.online)
	{
		test_game_screen = true;
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

		gui_state = GuiState::ButtonCheck;
	}

	ImGui::NextColumn();
	if (ImGui::Button("Start Training", ImVec2(150 * scaling, 50 * scaling)))
	{
		config::Delay = 0;
		settings.dojo.training = true;
		gui_state = GuiState::Closed;

		if (strlen(settings.content.path.data()) > 0)
		{
			std::string extension = get_file_extension(settings.content.path);
			// dreamcast games use built-in bios by default
			if (extension == "chd" || extension == "gdi" || extension == "cdi")
			{
				dojo_gui.bios_json_match = true;
				settings.platform.system = DC_PLATFORM_DREAMCAST;
			}
			else
			{
				int platform = naomi_cart_GetPlatform(settings.content.path.data());
				settings.platform.system = platform;
				//dojo_gui.bios_json_match = dojo_file.CompareBIOS(platform);
			}

			/*
			dojo_gui.current_json_match = dojo_file.CompareRom(settings.content.path);

			if (!dojo_gui.bios_json_match || !dojo_gui.current_json_match)
				gui_state = GuiState::BiosRomWarning;
			else
			*/

			gui_start_game(settings.content.path);
		}
		else
		{
			gui_state = GuiState::Main;
		}

	}
	ImGui::NextColumn();

	std::string filename = settings.content.path.substr(settings.content.path.find_last_of("/\\") + 1);
	std::string game_name = stringfix::remove_extension(filename);
	std::string net_state_path = get_writable_data_path(game_name + ".state.net");

	bool save_exists = false;
	if(ghc::filesystem::exists(net_state_path))
		save_exists = true;

	if(!save_exists)
	{
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	if (ImGui::Button("Delete Savestate", ImVec2(150 * scaling, 50 * scaling)))
	{
		if(ghc::filesystem::exists(net_state_path))
			ghc::filesystem::remove(net_state_path);
	}

	if(!save_exists)
	{
        ImGui::PopItemFlag();
        ImGui::PopStyleVar();
	}

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
	std::map<std::string, std::string> game_links = dojo_file.GetFileResourceLinks(settings.content.path);
	if (!game_links.empty())
	{
		for (std::pair<std::string, std::string> link: game_links)
		{
			ImGui::NextColumn();
			if (ImGui::Button(std::string("Open\n" + link.first).data(), ImVec2(150 * scaling, 50 * scaling)))
			{
#endif
#ifdef _WIN32
				ShellExecute(0, 0, link.second.data(), 0, 0, SW_SHOW);
#elif defined(__APPLE__)
				std::string cmd = "open " + link.second;
				system(cmd.data());
#elif defined(__linux__)
				std::string cmd = "xdg-open " + link.second;
				system(cmd.data());
#endif
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
			}
		}
	}
#endif

	ImGui::End();
}

std::vector<std::string> split(const std::string& text, char delimiter) {
    std::string tmp;
    std::vector<std::string> stk;
    std::stringstream ss(text);
    while(getline(ss,tmp, delimiter)) {
        stk.push_back(tmp);
    }
    return stk;
}

void DojoGui::gui_display_lobby_host_player_select(float scaling)
{
	std::string title = "Select # of Players";
	ImGui::OpenPopup(title.data());
	if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string num_slider_desc = "Adjust number of players.";
		if (!config::ActAsServer && config::NumPlayers > 2)
			num_slider_desc = num_slider_desc + " . Select your player below.";
		OptionSlider("# Players", config::NumPlayers, 2, 4, num_slider_desc.data());

		if (config::NumPlayers == 2)
		{
			config::ManualPlayerAssign = false;
		}
		else
		{
			config::ManualPlayerAssign = true;
		}

		if (ImGui::Button("Select Game"))
		{
			dojo.presence.client.EndSession();
			dojo.presence.player_count = 1;
			dojo.presence.hosting_lobby = true;
			std::thread t5(&LobbyClient::ClientThread, std::ref(dojo.presence.client));
			t5.detach();

			dojo.lobby_host_screen = true;
			config::ActAsServer = true;
			gui_state = GuiState::Main;
		}

		if (ImGui::Button("Cancel"))
		{
			gui_state = GuiState::Lobby;
		}
	}
}

void DojoGui::gui_display_lobby(float scaling, std::vector<GameMedia> game_list)
{
#ifndef __ANDROID__
	if (!dojo.lobby_active)
	{
		dojo.lobby_active = true;

		std::thread t4(&DojoLobby::ListenerThread, std::ref(dojo.presence));
		t4.detach();

		std::thread t5(&LobbyClient::ClientThread, std::ref(dojo.presence.client));
		t5.detach();
	}

	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("LAN Lobby", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		dojo.presence.lobby_disconnect_toggle = true;
		dojo.lobby_active = false;

		dojo.host_status = 0;
		dojo.lobby_host_screen = false;
		settings.content.path = "";

		dojo.beacon_active = false;
		dojo.lobby_launch = false;

		dojo_gui.item_current_idx = 0;

		config::NetworkServer.set("");
		config::NetworkP0Server.set("");
		config::NetworkP1Server.set("");
		config::NetworkP2Server.set("");
		config::NetworkP3Server.set("");

		if (game_started)
    		gui_state = GuiState::Commands;
    	else
    		gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Host Game").x - ImGui::GetStyle().FramePadding.x * 2.0f - ImGui::GetStyle().ItemSpacing.x);
	if (!dojo.lobby_host_screen && !dojo.beacon_active)
	{
		std::string title = "Select # of Players";
		if (ImGui::Button("Host Game", ImVec2(100 * scaling, 30 * scaling)))
		{
			ImGui::OpenPopup(title.data());
		}

		if (ImGui::BeginPopupModal(title.data(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
		{
			std::string num_slider_desc = "Adjust number of players.";
			OptionSlider("# Players", config::NumPlayers, 2, 4, num_slider_desc.data());

			if (config::NumPlayers == 2)
			{
				config::ManualPlayerAssign = false;
			}
			else
			{
				config::ManualPlayerAssign = true;
				config::PlayerNum = 0;
			}

			if (ImGui::Button("Select Game"))
			{
				dojo.presence.client.EndSession();
				dojo.presence.player_count = 1;
				dojo.presence.hosting_lobby = true;
				std::thread t5(&LobbyClient::ClientThread, std::ref(dojo.presence.client));
				t5.detach();

				dojo.lobby_host_screen = true;
				config::ActAsServer = true;
				gui_state = GuiState::Main;
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				config::NumPlayers = 2;
				config::ManualPlayerAssign = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}
	else
	{
		if (ImGui::Button("Cancel Host", ImVec2(100 * scaling, 30 * scaling)))
		{
			dojo.presence.lobby_disconnect_toggle = true;
			dojo.presence.CancelHost();
		}
	}

	ImGui::Columns(4, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::SetColumnWidth(0, ImGui::CalcTextSize("Ping").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x);
	//ImGui::Text("Ping"); ImGui::NextColumn();
	ImGui::Text("    "); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Status"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

	std::map<std::string, std::string> beacons = dojo.presence.active_beacons;
	for (auto it = beacons.cbegin(); it != beacons.cend(); ++it) {
		std::string s = it->second;
		std::string delimiter = "__";

		std::string beacon_id = it->first;
		std::vector<std::string> beacon_entry;

		if (dojo.presence.last_seen[beacon_id] + 10000 > dojo.unix_timestamp())
		{
			size_t pos = 0;
			std::string token;
			while ((pos = s.find(delimiter)) != std::string::npos) {
			    token = s.substr(0, pos);
			    //std::cout << token << std::endl;
				beacon_entry.push_back(token);
			    s.erase(0, pos + delimiter.length());
			}

			std::string beacon_ip = beacon_entry[0];
			std::string beacon_server_port = beacon_entry[1];
			std::string beacon_status = beacon_entry[2];

			std::string beacon_game = beacon_entry[3].c_str();
			std::string beacon_game_path = "";

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
				[&](GameMedia gm) { return ( gm.name.rfind(beacon_game, 0) == 0 ); });

			if (it != games.end())
			{
				beacon_game = it->name;
				beacon_game_path = it->path;
			}

			std::string beacon_player = beacon_entry[4];

			int beacon_player_num = std::stoi(beacon_entry[5]);
			if (beacon_player_num > 4)
				beacon_player_num = 4;
			else if (beacon_player_num < 2)
				beacon_player_num = 2;

			bool is_selected;
			int beacon_ping = dojo.presence.active_beacon_ping[beacon_id];
			std::string beacon_ping_str = "";
			if (beacon_ping > 0)
				beacon_ping_str = std::to_string(beacon_ping);

			if (beacon_status == "Hosting, Waiting" &&
				ImGui::Selectable(beacon_ping_str.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				std::string filename = beacon_game_path.substr(beacon_game_path.find_last_of("/\\") + 1);
				auto game_name = stringfix::remove_extension(filename);

				if (!ghc::filesystem::exists(get_writable_data_path(game_name + ".state.net")))
				{
					dojo_gui.invoke_download_save_popup(beacon_game_path, &dojo_gui.net_save_download, true);
				}
				else
				{
					settings.content.path = beacon_game_path;
					config::NetworkServer = beacon_ip;

					config::NumPlayers = beacon_player_num;
					if (beacon_player_num == 2)
						config::ManualPlayerAssign = false;
					else
						config::ManualPlayerAssign = true;
					dojo.presence.client.SendMsg(std::string("JOIN " + config::PlayerName.get() + ":" + std::to_string(config::GGPOPort.get())), beacon_ip, std::stoi(config::DojoServerPort.get()));
					dojo.host_status = 4;
				}
			}

			ImGui::NextColumn();

			ImGui::Text(beacon_player.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_status.c_str());  ImGui::NextColumn();
			ImGui::Text(beacon_game.c_str()); ImGui::NextColumn();
		}
	}

	lobby_player_wait_popup(game_list);

	if (dojo_gui.net_save_download)
	{
		ImGui::OpenPopup("Download Netplay Savestate");
	}

	dojo_gui.download_save_popup();

    ImGui::End();
    ImGui::PopStyleVar();
#endif

	if (dojo_file.start_save_download && !dojo_file.save_download_started)
	{
		dojo_file.save_download_started = true;
		std::thread s([&]() {
			if (config::Receiving)
			{
				if (dojo.replay_version == 2)
				{
					if(dojo_file.DownloadNetSave(dojo_file.entry_name, "9bd1161ec81a6636b1be9f7eabff381e70ad3ab8").empty())
					{
						// fall back to current savestate if commit not found
						dojo_file.DownloadNetSave(dojo_file.entry_name);
					}
				}
				else if (!settings.dojo.state_commit.empty())
				{
					std::cout << "state commit: " << settings.dojo.state_commit << std::endl;
					if(dojo_file.DownloadNetSave(dojo_file.entry_name, settings.dojo.state_commit).empty())
					{
						// fall back to current savestate if commit not found
						dojo_file.DownloadNetSave(dojo_file.entry_name);
					}
				}
				else
					dojo_file.DownloadNetSave(dojo_file.entry_name);
			}
			else
				dojo_file.DownloadNetSave(dojo_file.entry_name);
		});
		s.detach();
	}

}

void DojoGui::show_playback_menu(float scaling, bool paused)
{
	if (config::EnablePlayerNameOverlay)
		show_player_name_overlay(scaling, true);

	if (!config::ShowPlaybackControls)
	{
		return;
	}

	unsigned int total;
	if (dojo.replay_version == 1)
		total = dojo.net_inputs[0].size();
	else
		total = dojo.maple_inputs.size() - 1;

	int position = dojo.FrameNumber.load();

	if (!paused)
	{
		//ImGui_Impl_NewFrame();
		ImGui::NewFrame();
	}

	ImGui::SetNextWindowBgAlpha(0.6f);
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - 150, settings.display.height - 45));
	ImGui::SetNextWindowSize(ImVec2(350, 40));

	ImGui::Begin("##fn", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration );

	ImGui::SliderInt("", &position, 0, total);
	ImGui::SameLine();
	ImGui::Text("%u", total);

	ImGui::SameLine();

	/*
	if (dojo.replay_version == 1)
	{
		if (!paused)
		{
			if (ImGui::Button("Pause"))
			{
				TermAudio();
				gui_state = GuiState::ReplayPause;
			}

			ImGui::SameLine();
			if (ImGui::Button("Hide"))
			{
				config::ShowPlaybackControls = false;
			}
		}
		else
		{
			if (ImGui::Button("Play"))
			{
				InitAudio();
				gui_state = GuiState::Closed;
			}
		}
	}
	*/

	ImGui::End();

	if (!paused)
	{
		ImGui::Render();
	}
}

void DojoGui::display_btn(std::string btn_str, bool* any_found)
{
	std::vector<std::string> btns = { "1", "2", "3", "4", "5", "6",
			 "X", "Y", "LT", "A", "B", "RT",
			 "C", "Z", "D", "Start"};

	if (std::any_of(btns.begin(), btns.end(), [btn_str](std::string str){ return btn_str == str; }))
	{
		ImGui::SameLine();
		*any_found = true;
	}

	if (btn_str == "1")
		ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_ONE);
	else if (btn_str == "2")
		ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_TWO);
	else if (btn_str == "3")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_THREE);
	else if (btn_str == "4")
		ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s    ", ICON_KI_BUTTON_FOUR);
	else if (btn_str == "5")
		ImGui::TextColored(ImVec4(0, 175, 0, 1), "%s    ", ICON_KI_BUTTON_FIVE);
	else if (btn_str == "6")
		ImGui::TextColored(ImVec4(255, 0, 175, 1), "%s    ", ICON_KI_BUTTON_SIX);
	else if (btn_str == "X")
		ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
	else if (btn_str == "Y")
		ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
	else if (btn_str == "LT")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_L);
	else if (btn_str == "A")
		ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
	else if (btn_str == "B")
		ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
	else if (btn_str == "RT")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s", ICON_KI_BUTTON_R);
	else if (btn_str == "C")
		ImGui::TextColored(ImVec4(255, 255, 255, 1), "%s    ", ICON_KI_BUTTON_C);
	else if (btn_str == "Z")
		ImGui::TextColored(ImVec4(255, 75, 255, 1), "%s", ICON_KI_BUTTON_Z);
	else if (btn_str == "D")
		ImGui::TextColored(ImVec4(0, 0, 255, 1), "%s", ICON_KI_BUTTON_D);
	else if (btn_str == "Start")
		ImGui::Text("%s", ICON_KI_BUTTON_START);
}

void DojoGui::display_input_str(std::string input_str, std::string prev_str)
{
	bool any_found = false;
	std::vector<std::string> arcade_btns = {"1", "2", "3", "4", "5", "6", "C", "Z", "D", "Start"};
	std::vector<std::string> dc_btns = {"X", "Y", "LT", "A", "B", "RT", "C", "Start"};

	std::vector<std::string> buttons = dc_btns;
	if (settings.platform.isArcade())
		buttons = arcade_btns;

	std::string new_btns = "";
	if (prev_str.length() > 0)
	{
		for (int i = 0; i < buttons.size(); i++)
		{
			if (prev_str.find(buttons[i]) != std::string::npos &&
				input_str.find(buttons[i]) != std::string::npos)
				display_btn(buttons[i], &any_found);
			else if (prev_str.find(buttons[i]) == std::string::npos &&
					 input_str.find(buttons[i]) != std::string::npos)
				new_btns.append(buttons[i]);
		}
	}
	else
	{
		new_btns = input_str;
	}

	for (int i = 0; i < buttons.size(); i++)
	{
		if (new_btns.find(buttons[i]) != std::string::npos)
			display_btn(buttons[i], &any_found);
	}

	if (!any_found)
		ImGui::Text("");
}

void DojoGui::show_last_inputs_overlay()
{
	if (settings.dojo.training && config::Delay > 0)
		return;

	for (int di = 0; di < 2; di++)
	{
		if (!dojo.displayed_inputs[di].empty())
		{

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

#if defined(__APPLE__) || defined(__ANDROID__)
			ImGui::SetNextWindowSize(ImVec2(290, ImGui::GetIO().DisplaySize.y - 230));
#else
			ImGui::SetNextWindowSize(ImVec2(210, ImGui::GetIO().DisplaySize.y - 150));
#endif

			if (di == 0)
			{
#if defined(__APPLE__) || defined(__ANDROID__)
				ImGui::SetNextWindowPos(ImVec2(10, 180));
#else
				ImGui::SetNextWindowPos(ImVec2(10, 100));
#endif
				ImGui::SetNextWindowBgAlpha(0.4f);
				ImGui::Begin("#one_input", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
			}
			else if (di == 1)
			{
#if defined(__APPLE__) || defined(__ANDROID__)
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 300, 180));
#else
				ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 220, 100));
#endif
				ImGui::SetNextWindowBgAlpha(0.4f);
				ImGui::Begin("#two_input", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
			}

			if (dojo.displayed_inputs[di].size() > 60)
			{
				dojo.displayed_inputs[di].erase(dojo.displayed_inputs[di].begin());
				dojo.displayed_inputs_str[di].erase(dojo.displayed_inputs_str[di].begin());
				dojo.displayed_dirs_str[di].erase(dojo.displayed_dirs_str[di].begin());
				dojo.displayed_dirs[di].erase(dojo.displayed_dirs[di].begin());
				dojo.displayed_inputs_duration[di].erase(dojo.displayed_inputs_duration[di].begin());
				dojo.displayed_num_dirs[di].erase(dojo.displayed_num_dirs[di].begin());
			}

			std::map<u32, std::bitset<18>>::reverse_iterator it = dojo.displayed_inputs[di].rbegin();

			u32 input_frame_num = it->first;
			u32 input_duration = dojo.FrameNumber - input_frame_num;

			dojo.displayed_inputs_duration[di][input_frame_num] = input_duration;
			if (dojo.displayed_inputs_str[di].size() > 1)
			{
				it++;
				dojo.last_displayed_inputs_str[di] = dojo.displayed_inputs_str[di][it->first];
			}

			for(auto rit = dojo.displayed_inputs[di].rbegin(); rit != dojo.displayed_inputs[di].rend(); ++rit)
			{
				ImGui::Text("%03u", dojo.displayed_inputs_duration[di][rit->first]);
				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.594f, 0.806f, 0.912f, 1.f));
				if (config::UseAnimeInputNotation)
				{
					auto num = dojo.displayed_num_dirs[di][rit->first];
					ImGui::Text("%d", num);
				}
				else
				{
					auto num = dojo.displayed_num_dirs[di][rit->first];
					if (num == 1)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_LEFT);
					else if (num == 2)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM);
					else if (num == 3)
						ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_RIGHT);
					else if (num == 4)
						ImGui::Text("%s", ICON_KI_ARROW_LEFT);
					else if (num == 6)
						ImGui::Text("%s", ICON_KI_ARROW_RIGHT);
					else if (num == 7)
						ImGui::Text("%s", ICON_KI_ARROW_TOP_LEFT);
					else if (num == 8)
						ImGui::Text("%s", ICON_KI_ARROW_TOP);
					else if (num == 9)
						ImGui::Text("%s", ICON_KI_ARROW_TOP_RIGHT);
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
				display_input_str(dojo.displayed_inputs_str[di][rit->first], dojo.last_displayed_inputs_str[di]);
				dojo.last_displayed_inputs_str[di] = dojo.displayed_inputs_str[di][rit->first];
			}
			ImGui::End();

			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}
	}
}

void DojoGui::show_replay_position_overlay(int frame_num, float scaling, bool paused)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.FrameNumber < dojo.maple_inputs.size() ||
		dojo.FrameNumber < dojo.net_inputs[1].size() ||
		settings.dojo.training)
	{
		char text_pos[30] = { 0 };

		if (dojo.PlayMatch)
		{
			if (dojo.replay_version >= 2)
				sprintf(text_pos, "%u / %u     ", frame_num, dojo.maple_inputs.size());
			else
				sprintf(text_pos, "%u / %u     ", frame_num, dojo.net_inputs[1].size());
		}
		else if (settings.dojo.training)
		{
			sprintf(text_pos, "%u     ", frame_num);
		}

		float font_size = ImGui::CalcTextSize(text_pos).x;

		ImGui::SetNextWindowPos(ImVec2(settings.display.width - font_size, settings.display.height - 40));
		ImGui::SetNextWindowSize(ImVec2(font_size, 40));
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#pos", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		if (dojo.PlayMatch)
		{
			if (dojo.replay_version >= 2)
				ImGui::Text("%u / %u", frame_num, dojo.maple_inputs.size());
			else
				ImGui::Text("%u / %u", frame_num, dojo.net_inputs[1].size());
		}
		else if (settings.dojo.training)
		{
			ImGui::Text("%u", frame_num);
		}

		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::show_pause(float scaling)
{
	settings.input.fastForwardMode = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	//ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.475f, 0.825f, 1.000f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string pause_text;

	if (dojo.stepping)
		pause_text = "Stepping";
	else
	{
		if (dojo.buffering)
			pause_text = "Buffering";
		else
			pause_text = "Paused";
	}

	float font_size = ImGui::CalcTextSize(pause_text.c_str()).x;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size + 40) / 2), settings.display.height - 40));
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 40));
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#pause", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size + (font_size / 2) + 10
	);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::show_button_check(float scaling)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string pause_text;
	pause_text = "Button Check";

	float font_size = ImGui::CalcTextSize(pause_text.c_str()).x;

	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((font_size + 40) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 60));
#else
	ImGui::SetNextWindowSize(ImVec2(font_size + 40, 40));
#endif
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#button_check_title", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		font_size + (font_size / 2) + 10
	);

	ImGui::TextUnformatted(pause_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);

	int num_players = (ggpo_join_screen || test_game_screen) ? 1 : 2;
	for (int i = 0; i < num_players; i++)
	{
		if (num_players == 2)
		{
			if (i == 0)
				ImGui::SetNextWindowPos(ImVec2(settings.display.width / 4.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			else
				ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4.f) * 3, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		}
		else
		{
			ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2.f, settings.display.height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		}

		float font_height = ImGui::CalcTextSize("Test").y;

		if (settings.platform.isArcade())
			ImGui::SetNextWindowSize(ImVec2(130 * scaling, font_height * 18));
		else
			ImGui::SetNextWindowSize(ImVec2(130 * scaling, 0));

		std::string bc_title = "##button_check" + std::to_string(i);
		ImGui::Begin(bc_title.c_str(), NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

		auto areaWidth = ImGui::GetContentRegionAvail().x * 0.5f;

		if (ggpo_join_screen || test_game_screen)
		{
			std::string player_name = config::PlayerName.get();
			ImGui::SetCursorPosX(10.0f + areaWidth - (ImGui::CalcTextSize(player_name.c_str()).x / 2.0f));
			ImGui::Text("%s", player_name.c_str());
		}
		else
		{
			ImGui::SetCursorPosX(10.0f + areaWidth - (ImGui::CalcTextSize("Player X").x / 2.0f));
			ImGui::Text("Player %d", i + 1);
		}

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.594f, 0.806f, 0.912f, 1.f));

		int num = 0;
		if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1)
			num = 2;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 4;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 6;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1)
			num = 8;

		if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1 &&
			dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 1;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_DOWN) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 3;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_LEFT) == 1)
			num = 7;
		else if (dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_UP) == 1 &&
				 dojo.button_check_pressed[i].count(DreamcastKey::DC_DPAD_RIGHT) == 1)
			num = 9;

		ImGui::SetCursorPosX(areaWidth);

		if (num == 1)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_LEFT);
		else if (num == 2)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM);
		else if (num == 3)
			ImGui::Text("%s", ICON_KI_ARROW_BOTTOM_RIGHT);
		else if (num == 4)
			ImGui::Text("%s", ICON_KI_ARROW_LEFT);
		else if (num == 5)
			ImGui::Text("    ");
		else if (num == 6)
			ImGui::Text("%s", ICON_KI_ARROW_RIGHT);
		else if (num == 7)
			ImGui::Text("%s", ICON_KI_ARROW_TOP_LEFT);
		else if (num == 8)
			ImGui::Text("%s", ICON_KI_ARROW_TOP);
		else if (num == 9)
			ImGui::Text("%s", ICON_KI_ARROW_TOP_RIGHT);
		else
			ImGui::Text(" ");

		ImGui::PopStyleColor();

		ImGui::SetCursorPosX((areaWidth) * 0.5f);

		if (settings.platform.isArcade())
		{
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_BUTTON_C);
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_BUTTON_Z);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_A) == 1)
				ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_A);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_B) == 1)
				ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_B);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_C) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_BUTTON_C);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_C);

			ImGui::SetCursorPosX((areaWidth) * 0.5f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_X) == 1)
				ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_X);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Y) == 1)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Y);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Z) == 1)
				ImGui::TextColored(ImVec4(255, 0, 175, 1), "%s", ICON_KI_BUTTON_Z);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Z);

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x/2.0f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_START) == 1)
				ImGui::TextColored(ImVec4(165, 0, 255, 1), "%s", ICON_KI_CARET_TOP);
			else
				ImGui::Text("%s", ICON_KI_CARET_TOP);

			ImGui::Text(" ");

			std::set<int>::reverse_iterator rit;
			for (rit = dojo.button_check_pressed[i].rbegin(); rit != dojo.button_check_pressed[i].rend(); ++rit)
			{
				const char* button_name = GetCurrentGameButtonName((DreamcastKey)*rit);
				if (button_name != nullptr && strlen(button_name) > 0 && button_name != " ")
					ImGui::Text("%s\n", button_name);
			}
		}
		else
		{
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_X, ICON_KI_BUTTON_Y, ICON_KI_STICK_LEFT_TOP);
			//ImGui::Text("%s %s %s", ICON_KI_BUTTON_A, ICON_KI_BUTTON_B, ICON_KI_STICK_RIGHT_TOP);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_X) == 1)
				ImGui::TextColored(ImVec4(255, 255, 0, 1), "%s", ICON_KI_BUTTON_X);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_X);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_Y) == 1)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_Y);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_Y);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_AXIS_LT) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_STICK_LEFT_TOP);
			else
				ImGui::Text("%s", ICON_KI_STICK_LEFT_TOP);

			ImGui::SetCursorPosX((areaWidth) * 0.5f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_A) == 1)
				ImGui::TextColored(ImVec4(255, 0, 0, 1), "%s", ICON_KI_BUTTON_A);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_A);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_B) == 1)
				ImGui::TextColored(ImVec4(0, 175, 255, 1), "%s", ICON_KI_BUTTON_B);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_B);

			ImGui::SameLine();

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_AXIS_RT) == 1)
				ImGui::TextColored(ImVec4(255, 155, 0, 1), "%s", ICON_KI_STICK_RIGHT_TOP);
			else
				ImGui::Text("%s", ICON_KI_STICK_RIGHT_TOP);

			ImGui::SetCursorPosX(areaWidth - ImGui::CalcTextSize("A").x/2.0f);

			if (dojo.button_check_pressed[i].count(DreamcastKey::DC_BTN_START) == 1)
				ImGui::TextColored(ImVec4(165, 0, 255, 1), "%s", ICON_KI_CARET_TOP);
			else
				ImGui::Text("%s", ICON_KI_CARET_TOP);
		}
		ImGui::End();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.335f, 0.155f, 0.770f, 1.000f));

	std::string msg_text;
#if defined(__ANDROID__)
	msg_text = "Press MENU to exit.";
#else
	msg_text = "Press MENU or TAB to exit.";
#endif

	float msg_font_size = ImGui::CalcTextSize(msg_text.c_str()).x;

#if defined(__APPLE__) || defined(__ANDROID__)
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((msg_font_size + 40) / 2), settings.display.height - 60));
	ImGui::SetNextWindowSize(ImVec2(msg_font_size + 40, 60));
#else
	ImGui::SetNextWindowPos(ImVec2((settings.display.width / 2) - ((msg_font_size + 40) / 2), settings.display.height - 40));
	ImGui::SetNextWindowSize(ImVec2(msg_font_size + 40, 40));
#endif
	ImGui::SetNextWindowBgAlpha(0.65f);
	ImGui::Begin("#exit_description", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

	ImGui::SameLine(
		(ImGui::GetContentRegionAvail().x / 2) -
		msg_font_size + (msg_font_size / 2) + 10
	);

	ImGui::TextUnformatted(msg_text.c_str());

	ImGui::End();

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::show_player_name_overlay(float scaling, bool paused)
{
	// if both player names are defaults, hide overlay
	if (dojo.player_2.length() <= 1 ||
		strcmp(dojo.player_1.data(), "Player") == 0 &&
		strcmp(dojo.player_1.data(), dojo.player_2.data()) == 0)
	{
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

	if (dojo.player_1.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_1.data()).x;

		ImGui::SetNextWindowPos(ImVec2((settings.display.width / 4) - ((font_size + 20) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#one", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10
		);

		ImGui::TextUnformatted(dojo.player_1.c_str());

		ImGui::End();
	}

	if (dojo.player_2.length() > 1)
	{
		float font_size = ImGui::CalcTextSize(dojo.player_2.data()).x;

		ImGui::SetNextWindowPos(ImVec2(((settings.display.width / 4) * 3) - ((font_size + 20) / 2), 0));
#if defined(__APPLE__) || defined(__ANDROID__)
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 42));
#else
		ImGui::SetNextWindowSize(ImVec2(font_size + 20, 35));
#endif
		ImGui::SetNextWindowBgAlpha(0.5f);
		ImGui::Begin("#two", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);

		ImGui::SameLine(
			(ImGui::GetContentRegionAvail().x / 2) -
			font_size + (font_size / 2) + 10
		);

		ImGui::TextUnformatted(dojo.player_2.c_str());
		ImGui::End();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void DojoGui::gui_display_replay_pause(float scaling)
{
	if (settings.dojo.training && config::ShowInputDisplay ||
		dojo.PlayMatch && config::ShowReplayInputDisplay)
	{
		dojo_gui.show_last_inputs_overlay();
	}

	if (config::EnablePlayerNameOverlay)
		dojo_gui.show_player_name_overlay(scaling, false);
	dojo_gui.show_pause(scaling);
	dojo_gui.show_replay_position_overlay(dojo.FrameNumber, scaling, false);

	if (config::Receiving.get())
	{
		u32 current = dojo.FrameNumber.load();
		// RxFrameBuffer divided by 10 for some reason?
		u32 rx_buffer = (u32)(config::RxFrameBuffer.get() * 10);
		u32 resume_target = current + rx_buffer;

		if (config::BufferAutoResume.get() &&
			!dojo.manual_pause &&
			dojo.maple_inputs.size() > resume_target)
		{
			if (dojo.buffering)
				dojo.buffering = false;
			if (!config::ThreadedRendering)
			{
				gui_state = GuiState::Closed;
				emu.start();
			}
			else
			{
				gui_open_settings();
			}
		}
	}
}

void DojoGui::gui_display_paused(float scaling)
{
	show_playback_menu(scaling, true);
}

void DojoGui::gui_display_replays(float scaling, std::vector<GameMedia> game_list)
{
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(settings.display.width, settings.display.height));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGui::Begin("Replays", NULL, /*ImGuiWindowFlags_AlwaysAutoResize |*/ ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

	if (ImGui::Button("Done", ImVec2(100 * scaling, 30 * scaling)))
	{
		cfgSaveBool("dojo", "RecordMatches", config::RecordMatches);
		if (game_started)
			gui_state = GuiState::Commands;
		else
			gui_state = GuiState::Main;
	}

	ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Record All Sessions").x - ImGui::GetStyle().FramePadding.x * 4.0f - ImGui::GetStyle().ItemSpacing.x * 4);

	OptionCheckbox("Record All Sessions", config::RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all netplay sessions to a local file");

	ImGui::Columns(3, "mycolumns"); // 4-ways, with border
	ImGui::Separator();
	ImGui::Text("Date"); ImGui::NextColumn();
	ImGui::Text("Players"); ImGui::NextColumn();
	ImGui::Text("Game"); ImGui::NextColumn();
	ImGui::Separator();

#if defined(__WIN32__)
	fs::path path = fs::current_path() / "replays";
#else
	fs::path path = get_writable_data_path("") + "/replays";
#endif

	if (!ghc::filesystem::exists(path))
		ghc::filesystem::create_directory(path);

	std::map<std::string, std::string> replays;
	for (auto& p : fs::directory_iterator(path))
	{
		std::string replay_path = p.path().string();

		if (stringfix::get_extension(replay_path) == "flyreplay" ||
			stringfix::get_extension(replay_path) == "flyr")
		{

			std::string s = replay_path;
			std::string delimiter = "__";

			std::vector<std::string> replay_entry;

			size_t pos = 0;
			std::string token;
			while ((pos = s.find(delimiter)) != std::string::npos) {
				token = s.substr(0, pos);
				//std::cout << token << std::endl;
				replay_entry.push_back(token);
				s.erase(0, pos + delimiter.length());
			}

#ifdef _WIN32
			std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("\\") + 1);
#else
			std::string game_name = replay_entry[0].substr(replay_entry[0].rfind("/") + 1);
#endif

			std::string date = replay_entry[1];
			std::string host_player = replay_entry[2];
			std::string guest_player = replay_entry[3];

			std::string game_path = "";

			std::vector<GameMedia> games = game_list;
			std::vector<GameMedia>::iterator it = std::find_if(games.begin(), games.end(),
				[&](GameMedia gm) { return (gm.name.rfind(game_name, 0) == 0); });

			if (it != games.end())
			{
				game_path = it->path;
			}

			bool is_selected = false;
			if (ImGui::Selectable(date.c_str(), &is_selected, ImGuiSelectableFlags_SpanAllColumns))
			{
				dojo.ReplayFilename = replay_path;
				dojo.PlayMatch = true;

				gui_state = GuiState::Closed;
				//dojo.StartDojoSession();

				if (guest_player.empty())
					dojo.offline_replay = true;

				config::DojoEnable = true;
				gui_start_game(game_path);
			}
			ImGui::NextColumn();

			std::string players = host_player;
			if (!guest_player.empty())
				players += " vs " + guest_player;

			ImGui::TextUnformatted(players.c_str());  ImGui::NextColumn();
			ImGui::TextUnformatted(game_name.c_str());  ImGui::NextColumn();
		}
	}

    ImGui::End();
    ImGui::PopStyleVar();
}

inline static void header(const char *title)
{
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImGui::ButtonEx(title, ImVec2(-1, 0), ImGuiButtonFlags_Disabled);
	ImGui::PopStyleVar();
}

void DojoGui::insert_netplay_tab(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	{
		OptionCheckbox("Show Network Statistics Overlay", config::NetworkStats,
			"Display network statistics on screen");

		OptionCheckbox("Enable Player Name Overlay", config::EnablePlayerNameOverlay);
		ImGui::SameLine();
		ShowHelpMarker("Enable overlay showing player names during netplay sessions & replays");

		char PlayerName[256] = { 0 };
		strcpy(PlayerName, config::PlayerName.get().c_str());
		ImGui::InputText("Player Name", PlayerName, sizeof(PlayerName), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Name visible to other players");
		config::PlayerName = std::string(PlayerName, strlen(PlayerName));

		header("Chat");
		{
			OptionCheckbox("Enable Chat", config::GGPOChat, "Open the chat window when a chat message is received");
			if (config::GGPOChat)
			{
				OptionCheckbox("Enable Chat Window Timeout", config::GGPOChatTimeoutToggle, "Automatically close chat window after an assigned timeout after receiving message");
				if (config::GGPOChatTimeoutToggle)
				{
					char chatTimeout[256];
					sprintf(chatTimeout, "%d", (int)config::GGPOChatTimeout);
					ImGui::InputText("Timeout (seconds)", chatTimeout, sizeof(chatTimeout), ImGuiInputTextFlags_CharsDecimal, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Sets duration that chat window stays open after new message is received.");
					config::GGPOChatTimeout.set(atoi(chatTimeout));
					OptionCheckbox("Enable Chat Window Timeout On Send", config::GGPOChatTimeoutToggleSend, "Automatically close chat window after an assigned timeout after sending message");
				}
			}
		}

		header("Connection Methods");
		{
			int cxnMethod;
			if (!config::EnableMatchCode)
				cxnMethod = 0;
			else
				cxnMethod = 1;

			ImGui::Columns(2, "cxnMethod", false);
			ImGui::RadioButton("Direct IP", &cxnMethod, 0);
			ImGui::SameLine();
			ShowHelpMarker("Connect to IP Address.\nCan either be Public IP or Private IP via LAN.");
			ImGui::NextColumn();
			ImGui::RadioButton("Match Codes", &cxnMethod, 1);
			ImGui::SameLine();
			ShowHelpMarker("Establishes direct connection via public matchmaking relay.\nWorks with most home routers.");
			ImGui::NextColumn();
			ImGui::Columns(1, NULL, false);

			switch (cxnMethod)
			{
			case 0:
				config::EnableMatchCode = false;
				break;
			case 1:
				config::EnableMatchCode = true;
				break;
			}
		}

		OptionCheckbox("Enable LAN Lobby", config::EnableLobby,
			"Enable LAN Lobby interface. Works over any LAN or virtual LAN with multicast support.");

		if (config::EnableLobby)
		{
			if (ImGui::CollapsingHeader("LAN Lobby", ImGuiTreeNodeFlags_None))
			{
				char LobbyMulticastAddress[256];

				strcpy(LobbyMulticastAddress, config::LobbyMulticastAddress.get().c_str());
				ImGui::InputText("Multicast Address", LobbyMulticastAddress, sizeof(LobbyMulticastAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::LobbyMulticastAddress = LobbyMulticastAddress;

				char LobbyMulticastPort[256];

				strcpy(LobbyMulticastPort, config::LobbyMulticastPort.get().c_str());
				ImGui::InputText("Multicast Port", LobbyMulticastPort, sizeof(LobbyMulticastPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::LobbyMulticastPort = LobbyMulticastPort;
			}
		}

		if (config::EnableMatchCode)
		{
			if (ImGui::CollapsingHeader("Match Codes##MCHeader", ImGuiTreeNodeFlags_None))
			{
				char MatchmakingServerAddress[256];

				strcpy(MatchmakingServerAddress, config::MatchmakingServerAddress.get().c_str());
				ImGui::InputText("Matchmaking Service Address", MatchmakingServerAddress, sizeof(MatchmakingServerAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::MatchmakingServerAddress = MatchmakingServerAddress;

				char MatchmakingServerPort[256];

				strcpy(MatchmakingServerPort, config::MatchmakingServerPort.get().c_str());
				ImGui::InputText("Matchmaking Service Port", MatchmakingServerPort, sizeof(MatchmakingServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				config::MatchmakingServerPort = MatchmakingServerPort;
			}
		}

		if (config::NetplayMethod.get() == "GGPO")
		{
			if (!config::EnableMatchCode)
			{
				OptionCheckbox("Show Public IP on Connection Dialog", config::ShowPublicIP);
			}

			if (ImGui::CollapsingHeader("GGPO", ImGuiTreeNodeFlags_None))
			{
				int GGPOPort = config::GGPOPort.get();
				ImGui::InputInt("GGPO Local Port", &GGPOPort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO port to listen on");
				if (GGPOPort != config::GGPOPort.get())
					config::GGPOPort = GGPOPort;

				int GGPORemotePort = config::GGPORemotePort.get();
				ImGui::InputInt("GGPO Remote Port", &GGPORemotePort);
				ImGui::SameLine();
				ShowHelpMarker("The GGPO destination port to transmit to");
				if (GGPORemotePort != config::GGPORemotePort.get())
					config::GGPORemotePort = GGPORemotePort;

				std::string PortTitle;
				std::string PortDescription;

				if (config::NetplayMethod.get() == "Delay")
				{
					PortTitle = "Server Port";
					PortDescription = "The server port to listen on";
				}
				else
				{
					PortTitle = "Handshake Port";
					PortDescription = "The handshake port to listen on";
				}

				char ServerPort[256];
				strcpy(ServerPort, config::DojoServerPort.get().c_str());

				ImGui::InputText(PortTitle.c_str(), ServerPort, sizeof(ServerPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
				ImGui::SameLine();
				ShowHelpMarker(PortDescription.c_str());
				config::DojoServerPort = ServerPort;

				ImGui::Text("Left Thumbstick Simulation (Dreamcast):");
				ImGui::SameLine();
				ShowHelpMarker("Simulates Dreamcast Left Thumbstick. If unneeded, leave disabled. May cause extra rollbacks online.");
				OptionRadioButton<int>("Disabled", config::GGPOAnalogAxes, 0, "Left thumbstick not used. Recommended for most games.");
				ImGui::SameLine();
				OptionRadioButton<int>("Horizontal", config::GGPOAnalogAxes, 1, "Use the left thumbstick horizontal axis only. Used for racing games.");
				ImGui::SameLine();
				OptionRadioButton<int>("Full", config::GGPOAnalogAxes, 2, "Use the left thumbstick horizontal and vertical axes. Used for games that require full analog to function.");
			}

			if (ImGui::CollapsingHeader("Permissions", ImGuiTreeNodeFlags_None))
			{
#ifdef _WIN32
				OptionCheckbox("Enable Windows Firewall Policy", config::EnableWinFWPolicy);
				ImGui::SameLine();
				ShowHelpMarker("Enables programmatic Windows Defender Firewall policies. Authorizes application. Opens ports used for GGPO & LAN Lobby/Match Codes.");
#endif
				OptionCheckbox("Enable UPnP", config::EnableUPnP);
				ImGui::SameLine();
				ShowHelpMarker("Enable Universal Plug & Play for game sessions.");
			}
		}

		if (!config::EnableMatchCode && config::NetplayMethod.get() == "Delay")
		{
			OptionCheckbox("Enable LAN Lobby", config::EnableLobby);
			ImGui::SameLine();
			ShowHelpMarker("Enable discovery and matchmaking over LAN");

			if (config::EnableLobby)
			{
				if (ImGui::CollapsingHeader("LAN Lobby", ImGuiTreeNodeFlags_DefaultOpen))
				{
					char LobbyMulticastAddress[256];

					strcpy(LobbyMulticastAddress, config::LobbyMulticastAddress.get().c_str());
					ImGui::InputText("Lobby Multicast Address", LobbyMulticastAddress, sizeof(LobbyMulticastAddress), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Multicast IP Address for Lobby to Target");
					config::LobbyMulticastAddress = LobbyMulticastAddress;

					char LobbyMulticastPort[256];

					strcpy(LobbyMulticastPort, config::LobbyMulticastPort.get().c_str());
					ImGui::InputText("Lobby Multicast Port", LobbyMulticastPort, sizeof(LobbyMulticastPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
					ImGui::SameLine();
					ShowHelpMarker("Multicast Port for Lobby to Target");
					config::LobbyMulticastPort = LobbyMulticastPort;
				}
			}
		}

		if (ImGui::CollapsingHeader("Memory Management", ImGuiTreeNodeFlags_None))
		{
			OptionCheckbox("Validate BIOS & ROMs before netplay session", config::NetStartVerifyRoms);
			ImGui::SameLine();
			ShowHelpMarker("Validates BIOSes & ROMs against provided flycast_roms.json. Ensures proper files for retrieved savestates and known netplay games.");

			OptionCheckbox("Enable NVMEM/EEPROM Restoration", config::EnableMemRestore);
			ImGui::SameLine();
			ShowHelpMarker("Restores NVMEM & EEPROM files before netplay session to prevent desyncs. Disable if you wish to use modified files with your opponent. (i.e., palmods, custom dipswitches)");

			if (config::NetplayMethod.get() == "Delay")
			{
				OptionCheckbox("Ignore Existing Netplay Savestates", config::IgnoreNetSave);
				ImGui::SameLine();
				ShowHelpMarker("Ignore previously generated or custom savestates ending in .net.");
			}

			OptionCheckbox("Allow Custom VMUs", config::NetCustomVmu);
			ImGui::SameLine();
			ShowHelpMarker("Allows custom VMUs for netplay ending in .bin.net. VMU must match opponent's. Deletes and regenerates blank Dreamcast VMUs for netplay when disabled.");

			char NetSaveBase[256];

			strcpy(NetSaveBase, config::NetSaveBase.get().c_str());
			ImGui::InputText("Savestate Repository URL", NetSaveBase, sizeof(NetSaveBase), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			config::NetSaveBase = NetSaveBase;
		}

		if (config::NetplayMethod.get() == "Delay")
		{
			if (ImGui::CollapsingHeader("Advanced", ImGuiTreeNodeFlags_None))
			{
				OptionCheckbox("Enable Session Quick Load", config::EnableSessionQuickLoad);
				ImGui::SameLine();
				ShowHelpMarker("Saves state at common frame after boot. Allows you to press 'Quick Load' to revert to common state for both players. (Manually set on both sides)");

				ImGui::SliderInt("Packets Per Frame", (int*)&config::PacketsPerFrame.get(), 1, 10);
				ImGui::SameLine();
				ShowHelpMarker("Number of packets to send per input frame.");

				OptionCheckbox("Enable Backfill", config::EnableBackfill);
				ImGui::SameLine();
				ShowHelpMarker("Transmit past input frames along with current one in packet payload. Aids in unreliable connections.");

				if (config::EnableBackfill)
				{
					ImGui::SliderInt("Number of Past Input Frames", (int*)&config::NumBackFrames.get(), 1, 40);
					ImGui::SameLine();
					ShowHelpMarker("Number of past inputs to send per frame.");
				}
			}
		}

	}

	ImGui::PopStyleVar();
}

void DojoGui::insert_replays_tab(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);

	OptionCheckbox("Show Frame Position", config::ShowPlaybackControls);
	ImGui::SameLine();
	ShowHelpMarker("Shows current frame position on playback.");

	OptionCheckbox("Record All Sessions", config::RecordMatches);
	ImGui::SameLine();
	ShowHelpMarker("Record all gameplay sessions to a local file");

	OptionCheckbox("Show Input Display", config::ShowReplayInputDisplay);
	ImGui::SameLine();
	ShowHelpMarker("Shows controller input history in replays");

	header("Session Streaming");
	{
		OptionCheckbox("Enable Session Transmission", config::Transmitting);
		ImGui::SameLine();
		ShowHelpMarker("Transmit netplay sessions as TCP stream to target spectator");

		if (config::Transmitting)
		{
			char SpectatorIP[256];

			strcpy(SpectatorIP, config::SpectatorIP.get().c_str());
			ImGui::InputText("Spectator IP Address", SpectatorIP, sizeof(SpectatorIP), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
			ImGui::SameLine();
			ShowHelpMarker("Target Spectator IP Address");
			config::SpectatorIP = SpectatorIP;

			/*
			OptionCheckbox("Transmit Replays", config::TransmitReplays);
			ImGui::SameLine();
			ShowHelpMarker("Transmit replays to target spectator");
			*/
		}

		char SpectatorPort[256];

		strcpy(SpectatorPort, config::SpectatorPort.get().c_str());
		ImGui::InputText("Spectator Port", SpectatorPort, sizeof(SpectatorPort), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Port to send or receive session streams");
		config::SpectatorPort = SpectatorPort;

		int one = 1;
		ImGui::InputScalar("Frame Buffer", ImGuiDataType_S32, &config::RxFrameBuffer.get(), &one, NULL, "%d");
		ImGui::SameLine();
		ShowHelpMarker("# of frames to cache before playing received match stream");
	}

	ImGui::PopStyleVar();
}

void DojoGui::insert_training_tab(ImVec2 normal_padding)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, normal_padding);
	{
		OptionCheckbox("Show Input Display", config::ShowInputDisplay);
		ImGui::SameLine();
		ShowHelpMarker("Shows controller input history in Training Mode\n(Temporarily disabled for Offline Delay > 0)");

		if (config::ShowInputDisplay)
		{
			OptionCheckbox("Use Numpad Notation", config::UseAnimeInputNotation);
			ImGui::SameLine();
			ShowHelpMarker("Show inputs using Numpad/Anime Notation for Directions");
		}

		OptionCheckbox("Hide Random Input Slot", config::HideRandomInputSlot);
		ImGui::SameLine();
		ShowHelpMarker("Hides input slot is being played for random playback");

		OptionCheckbox("Start Recording on First Input", config::RecordOnFirstInput);
		ImGui::SameLine();
		ShowHelpMarker("Delay dummy recording until the first input is registered");
	}

	ImGui::PopStyleVar();
}

void DojoGui::update_action()
{
	if (ImGui::BeginPopupModal("Update?", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		std::string tag_name;
		std::string download_url;

		std::tie(tag_name, download_url) = dojo_file.tag_download;

		if (strcmp(tag_name.data(), GIT_VERSION) != 0)
		{
			ImGui::Text("There is a new version of Flycast Dojo available.\nWould you like to Update?");

			if (ImGui::BeginPopupModal("Update", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
			{
				ImGui::TextUnformatted(dojo_file.status_text.data());
				if (strcmp(dojo_file.status_text.data(), "Update complete.\nPlease restart Flycast Dojo to use new version.") == 0)
				{
					if (ImGui::Button("Exit"))
					{
						exit(0);
					}
				}
				else
				{
					float progress 	= float(dojo_file.downloaded_size) / float(dojo_file.total_size);
					char buf[32];
					sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
					ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
				}
				ImGui::EndPopup();
			}

			if (ImGui::Button("Update"))
			{
				ImGui::OpenPopup("Update");
				dojo_file.start_update = true;
			}
			ImGui::SameLine();
		}
		else
		{
			ImGui::Text("Flycast Dojo is already on the newest version.");
		}

		if (ImGui::Button("Close"))
		{
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	if (dojo_file.start_update && !dojo_file.update_started)
	{
		std::thread t([&]() {
			dojo_file.Update();
		});
		t.detach();
	}

}

void TextCenter(std::string text)
{
	float font_size = ImGui::GetFontSize() * text.size();

	ImGui::SameLine(
		ImGui::GetContentRegionAvail().x / 2 -
		font_size + (font_size / 2)
	);

	ImGui::Text(text.c_str());
}

void DojoGui::lobby_player_wait_popup(std::vector<GameMedia> game_list)
{
	std::string filename = settings.content.path.substr(settings.content.path.find_last_of("/\\") + 1);
	auto game_name = stringfix::remove_extension(filename);

	std::vector<GameMedia> games = game_list;
	std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
		[&](GameMedia gm) { return ( gm.name.rfind(game_name, 0) == 0 ); });

	std::string game_fullname = it->name;

	std::string lobby_title;
	if (dojo.host_status == 1)
		lobby_title = "Hosting " + game_fullname;
	else if (dojo.host_status == 4)
		lobby_title = "Joining " + game_fullname;

	if (dojo.host_status == 1 || dojo.host_status == 4)
	{
		ImGui::OpenPopup(lobby_title.c_str());
	}

	//centerNextWindow();
	ImGui::SetNextWindowSize(ScaledVec2(ImGui::CalcTextSize(lobby_title.c_str()).x + 20, 0));

	if (ImGui::BeginPopupModal(lobby_title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		int joined_player_count;
		if (dojo.host_status == 1)
			joined_player_count = dojo.presence.player_count;
		else
			joined_player_count = dojo.presence.player_count + 1;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(10, 10));
		ImGui::AlignTextToFramePadding();
		ImGui::SetCursorPosX(20.f * settings.display.uiScale);

		ImGui::Text(" ");
		ImGui::SameLine(
			(ImGui::CalcTextSize(lobby_title.c_str()).x) / 2 -
			200 + (200 / 2)
		);
		displayGameImage(game_name, ImVec2(200, 200), game_list);

		ImGui::Text(" ");
		ImGui::SameLine(
			(ImGui::CalcTextSize(lobby_title.c_str()).x + 30) / 2 -
			ImGui::CalcTextSize("Waiting for Players...").x + (ImGui::CalcTextSize("Waiting for Players...").x / 2)
		);
		ImGui::Text("Waiting for Players...");

		std::string space = "  ";
		for (int i=0; i<config::NumPlayers; i++)
		{
			space += "  ";
		}

		ImGui::Text(" ");
		ImGui::SameLine(
			(ImGui::CalcTextSize(lobby_title.c_str()).x) / 2 -
			ImGui::CalcTextSize(space.c_str()).x + (ImGui::CalcTextSize(space.c_str()).x / 2)
		);
		ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_ONE);
		ImGui::SameLine();
		if (joined_player_count >= 2)
			ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_TWO);
		else
			ImGui::Text("%s", ICON_KI_BUTTON_TWO);

		if (config::NumPlayers >= 3)
		{
			ImGui::SameLine();
			if (joined_player_count >= 3)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_THREE);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_THREE);
		}

		if (config::NumPlayers == 4)
		{
			ImGui::SameLine();
			if (joined_player_count == 4)
				ImGui::TextColored(ImVec4(0, 255, 0, 1), "%s", ICON_KI_BUTTON_FOUR);
			else
				ImGui::Text("%s", ICON_KI_BUTTON_FOUR);
		}

		ImGui::Text(" ");
		ImGui::SameLine(
			(ImGui::CalcTextSize(lobby_title.c_str()).x + 20) / 2 -
			ImGui::CalcTextSize("Cancel").x + (ImGui::CalcTextSize("Cancel").x / 2)
		);
		if (ImGui::Button("Cancel"))
		{
			config::NumPlayers = 2;
			config::ManualPlayerAssign = false;
			dojo.presence.CancelHost();
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleVar();

		ImGui::EndPopup();
	}

}

void DojoGui::displayGameImage(std::string game_name, ImVec2 size, std::vector<GameMedia> game_list)
{
		std::vector<GameMedia> games = game_list;
		std::vector<GameMedia>::iterator it = std::find_if (games.begin(), games.end(),
			[&](GameMedia gm) { return ( gm.name.rfind(game_name, 0) == 0 ); });

		GameMedia game = *it;
		ImTextureID textureId{};
		Boxart boxart;
		GameBoxart art = boxart.getBoxart(game);
		getGameImage(art, textureId, true);

		float ar = imguiDriver->getAspectRatio(textureId);
		ImVec2 uv0 { 0.f, 0.f };
		ImVec2 uv1 { 1.f, 1.f };
		if (ar > 1)
		{
			uv0.y = -(ar - 1) / 2;
			uv1.y = 1 + (ar - 1) / 2;
		}
		else if (ar != 0)
		{
			ar = 1 / ar;
			uv0.x = -(ar - 1) / 2;
			uv1.x = 1 + (ar - 1) / 2;
		}

		ImGui::Image(textureId, ImVec2(250, 250), uv0, uv1);
}

bool DojoGui::getGameImage(const GameBoxart& art, ImTextureID& textureId, bool allowLoad)
{
	textureId = ImTextureID{};
	if (art.boxartPath.empty())
		return false;

	// Get the boxart texture. Load it if needed.
	textureId = imguiDriver->getTexture(art.boxartPath);
	if (textureId == ImTextureID() && allowLoad)
	{
		int width, height;
		u8 *imgData = loadImage(art.boxartPath, width, height);
		if (imgData != nullptr)
		{
			try {
				textureId = imguiDriver->updateTextureAndAspectRatio(art.boxartPath, imgData, width, height);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
		return true;
	}
	return false;
}


void DojoGui::invoke_download_save_popup(std::string game_path, bool* net_save_download, bool launch_game)
{
	std::string filename = game_path.substr(game_path.find_last_of("/\\") + 1);
	std::string short_game_name = stringfix::remove_extension(filename);
	dojo_file.entry_name = short_game_name;
	dojo_file.post_save_launch = launch_game;
	dojo_file.start_save_download = true;
	dojo_file.game_path = game_path;

	*net_save_download = true;
}

void DojoGui::download_save_popup()
{
	if (ImGui::BeginPopupModal("Download Netplay Savestate", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiInputTextFlags_EnterReturnsTrue))
	{
		if(!dojo_file.save_download_ended)
		{
			ImGui::TextUnformatted(dojo_file.status_text.data());
			if (dojo_file.downloaded_size == dojo_file.total_size && dojo_file.save_download_ended
				|| dojo_file.status_text.find("not found") != std::string::npos)
			{
				dojo_file.start_save_download = false;
				dojo_file.save_download_started = false;
			}
			else
			{
				float progress = float(dojo_file.downloaded_size) / float(dojo_file.total_size);
				char buf[32];
				sprintf(buf, "%d/%d", (int)(progress * dojo_file.total_size), dojo_file.total_size);
				ImGui::ProgressBar(progress, ImVec2(0.f, 0.f), buf);
			}

			if (dojo_file.status_text.find("not found") != std::string::npos)
			{
				for (int i = 0; i < 16; i++)
				{
					settings.network.md5.savestate[i] = 0;
				}
			}
		}

		if (dojo_file.save_download_ended || dojo_file.status_text.find("not found") != std::string::npos)
		{
			ImGui::TextUnformatted("Savestate successfully downloaded. ");
			if (config::Receiving)
			{
				ImGui::TextUnformatted("Please open the replay link again to continue.");
				if (ImGui::Button("Exit"))
				{
					exit(0);
				}
			}
			else if (dojo_file.post_save_launch)
			{
				if (dojo.lobby_host_screen)
				{
					if (ImGui::Button("Back to Host Game Selection"))
					{
						net_save_download = false;
						ImGui::CloseCurrentPopup();
					}
				}
				else if (gui_state == GuiState::Lobby)
				{
					if (ImGui::Button("Back to Join Game Selection"))
					{
						net_save_download = false;
						ImGui::CloseCurrentPopup();
					}
				}
				else
				{
					if (ImGui::Button("Launch Game"))
					{
						dojo_file.entry_name = "";
						dojo_file.save_download_ended = false;
						dojo_file.post_save_launch = false;
						std::string game_path = dojo_file.game_path;
						settings.content.path = game_path;
						dojo_file.game_path = "";
						ImGui::CloseCurrentPopup();
						gui_start_game(game_path);
					}
				}
			}
			else
			{
				if (ImGui::Button("Close"))
				{
					dojo_file.entry_name = "";
					dojo_file.save_download_ended = false;
					ImGui::CloseCurrentPopup();
					scanner_refresh();
				}
			}
		}

		ImGui::EndPopup();
	}
}

DojoGui dojo_gui;
