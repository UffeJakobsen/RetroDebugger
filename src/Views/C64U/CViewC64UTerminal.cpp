#include "CViewC64UTerminal.h"
#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/Transport/C64UTelnetClient.h"
#include "../../Tools/C64SettingsStorage.h"
#include "CSlrString.h"
#include "imgui.h"
#include "DBG_Log.h"

#include <thread>

extern int c64SettingsC64UTelnetPort;
extern CSlrString *c64SettingsC64UHostname;

CViewC64UTerminal::CViewC64UTerminal(const char *name, float posX, float posY, float posZ,
									 float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiViewTerminal(name, posX, posY, posZ, sizeX, sizeY, 80, 25)
{
	this->debugInterface = debugInterface;
	this->autoConnectAttempted = false;

	telnetClient = new C64UTelnetClient();

	// Wire callbacks: terminal output -> telnet send
	SetWriteCallback([this](const uint8_t *data, size_t len) {
		telnetClient->Send(data, len);
	});

	// Wire callbacks: telnet data -> terminal input
	telnetClient->SetDataCallback([this](const uint8_t *data, size_t len) {
		ProcessInput(data, len);
	});
}

CViewC64UTerminal::~CViewC64UTerminal()
{
	telnetClient->Disconnect();
	delete telnetClient;
}

void CViewC64UTerminal::RenderImGui()
{
	// Auto-connect when C64U is connected and telnet is not
	if (debugInterface->isRunning && !telnetClient->IsConnected() && !autoConnectAttempted)
	{
		if (c64SettingsC64UHostname)
		{
			char *host = c64SettingsC64UHostname->GetStdASCII();
			LOGD("CViewC64UTerminal: auto-connecting to %s:%d", host, c64SettingsC64UTelnetPort);

			std::string hostStr(host);
			int port = c64SettingsC64UTelnetPort;
			delete[] host;

			// Connect in background to avoid blocking render
			std::thread([this, hostStr, port]() {
				telnetClient->Connect(hostStr, port);
			}).detach();

			autoConnectAttempted = true;
		}
	}

	// Reset auto-connect flag when C64U disconnects
	if (!debugInterface->isRunning)
	{
		if (telnetClient->IsConnected())
			telnetClient->Disconnect();
		autoConnectAttempted = false;
	}

	// Show connection status before terminal
	if (!telnetClient->IsConnected())
	{
		PreRenderImGui();
		ImGui::TextDisabled("Telnet not connected");
		if (debugInterface->isRunning)
		{
			if (ImGui::Button("Connect"))
			{
				if (c64SettingsC64UHostname)
				{
					char *host = c64SettingsC64UHostname->GetStdASCII();
					std::string hostStr(host);
					int port = c64SettingsC64UTelnetPort;
					delete[] host;
					std::thread([this, hostStr, port]() {
						telnetClient->Connect(hostStr, port);
					}).detach();
				}
			}
		}
		PostRenderImGui();
		return;
	}

	// Render terminal
	CGuiViewTerminal::RenderImGui();
}
