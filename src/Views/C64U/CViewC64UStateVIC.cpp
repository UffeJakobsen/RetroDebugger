#include "CViewC64UStateVIC.h"

#include "../../Emulators/c64u/CDebugInterfaceC64U.h"
#include "../../Emulators/c64u/State/C64ULogicalStateCache.h"

#include "imgui.h"

CViewC64UStateVIC::CViewC64UStateVIC(const char *name, float posX, float posY, float posZ, float sizeX, float sizeY, CDebugInterfaceC64U *debugInterface)
	: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	this->debugInterface = debugInterface;
}

void CViewC64UStateVIC::Render()
{
}

void CViewC64UStateVIC::RenderImGui()
{
	PreRenderImGui();

	C64ULogicalStateCache *stateCache = debugInterface->GetLogicalStateCache();
	if (stateCache == NULL)
	{
		ImGui::TextDisabled("State not available");
		PostRenderImGui();
		return;
	}

	C64UVicState vic = stateCache->GetVicState();
	C64UBankState bank = stateCache->GetBankState();

	// Mode header
	ImGui::Text("Mode: %s", vic.GetModeName());
	ImGui::SameLine();
	if (vic.IsDisplayEnabled())
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[DEN]");
	else
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[DEN off]");

	ImGui::Separator();

	// Display settings
	ImGui::Text("Raster: %03d  Scroll: Y=%d X=%d",
		vic.GetRasterLine(), vic.GetYScroll(), vic.GetXScroll());
	ImGui::Text("Border: $%X  BG0: $%X  BG1: $%X  BG2: $%X  BG3: $%X",
		vic.GetBorderColor(), vic.GetBackgroundColor(0),
		vic.GetBackgroundColor(1), vic.GetBackgroundColor(2),
		vic.GetBackgroundColor(3));

	// Memory layout
	if (ImGui::TreeNodeEx("Memory Layout", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("VIC Bank: $%04X-$%04X", bank.GetVicBank(), bank.GetVicBank() + 0x3FFF);
		ImGui::Text("Screen:   $%04X", bank.screenAddress);
		ImGui::Text("Charset:  $%04X", bank.charsetAddress);
		ImGui::Text("Bitmap:   $%04X", bank.bitmapAddress);
		ImGui::TreePop();
	}

	// Sprites
	if (ImGui::TreeNode("Sprites"))
	{
		if (ImGui::BeginTable("SpriteTable", 9, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			ImGui::TableSetupColumn("#");
			ImGui::TableSetupColumn("En");
			ImGui::TableSetupColumn("X");
			ImGui::TableSetupColumn("Y");
			ImGui::TableSetupColumn("MC");
			ImGui::TableSetupColumn("XE");
			ImGui::TableSetupColumn("YE");
			ImGui::TableSetupColumn("Pri");
			ImGui::TableSetupColumn("Col");
			ImGui::TableHeadersRow();

			for (int i = 0; i < 8; i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::Text("%d", i);
				ImGui::TableNextColumn();
				if (vic.IsSpriteEnabled(i))
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Y");
				else
					ImGui::TextDisabled("N");
				ImGui::TableNextColumn(); ImGui::Text("%3d", vic.GetSpriteX(i));
				ImGui::TableNextColumn(); ImGui::Text("%3d", vic.GetSpriteY(i));
				ImGui::TableNextColumn(); ImGui::Text("%s", vic.IsSpriteMulticolor(i) ? "Y" : "N");
				ImGui::TableNextColumn(); ImGui::Text("%s", vic.IsSpriteXExpand(i) ? "Y" : "N");
				ImGui::TableNextColumn(); ImGui::Text("%s", vic.IsSpriteYExpand(i) ? "Y" : "N");
				ImGui::TableNextColumn(); ImGui::Text("%s", vic.IsSpriteBgPriority(i) ? "BG" : "FG");
				ImGui::TableNextColumn(); ImGui::Text("$%X", vic.GetSpriteColor(i));
			}
			ImGui::EndTable();
		}
		ImGui::Text("MC0: $%X  MC1: $%X", vic.GetSpriteMulticolor0(), vic.GetSpriteMulticolor1());
		ImGui::TreePop();
	}

	// Raw registers (collapsed by default)
	if (ImGui::TreeNode("Raw Registers"))
	{
		for (int row = 0; row < 4; row++)
		{
			ImGui::Text("$D0%X0:", row);
			ImGui::SameLine();
			for (int col = 0; col < 16; col++)
			{
				int regIdx = row * 16 + col;
				if (regIdx < 0x40)
				{
					ImGui::SameLine();
					ImGui::Text("%02X", vic.registers[regIdx]);
				}
			}
		}
		ImGui::TreePop();
	}

	PostRenderImGui();
}
