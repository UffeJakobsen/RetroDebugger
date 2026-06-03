#include "CDebugBreakpointsData.h"
#include "CViewDisassembly.h"
#include "CDebugInterface.h"
#include "CDebugSymbols.h"
#include "CDebugSymbolsSegment.h"
#include "CDebugSymbolsCodeLabel.h"
#include "GUI_Main.h"

#include "CDebugBreakpointEventCallback.h"
#include "CDebuggerServer.h"
#include "CViewC64.h"

const char *comparisonMethodsStr[MEMORY_BREAKPOINT_ARRAY_SIZE] = { "==", "!=", "<", "<=", ">", ">=" };

static bool MemoryBreakpointValueMatches(int value, int target, DataBreakpointComparison comparison)
{
	switch (comparison)
	{
		case DataBreakpointComparison::MEMORY_BREAKPOINT_EQUAL:			return value == target;
		case DataBreakpointComparison::MEMORY_BREAKPOINT_NOT_EQUAL:		return value != target;
		case DataBreakpointComparison::MEMORY_BREAKPOINT_LESS:			return value <  target;
		case DataBreakpointComparison::MEMORY_BREAKPOINT_LESS_OR_EQUAL:	return value <= target;
		case DataBreakpointComparison::MEMORY_BREAKPOINT_GREATER:		return value >  target;
		case DataBreakpointComparison::MEMORY_BREAKPOINT_GREATER_OR_EQUAL: return value >= target;
		default: return false;
	}
}

CDebugBreakpointData *CDebugBreakpointsData::EvaluateBreakpoint(int addr, int value, u32 memoryAccess)
{
	// Iterate the full breakpoint set: single-address entries match only their
	// own addr, range entries (addrEnd > addr) match any addr inside the range.
	// breakpoints is keyed by start address, so std::map order helps short-circuit
	// once the start exceeds addr — but n is tiny in practice so a linear sweep
	// is fine and simpler.
	CDebugBreakpointData *evaluateBreakpoint = NULL;
	for (std::map<int, CDebugBreakpointAddr *>::iterator it = breakpoints.begin(); it != breakpoints.end(); it++)
	{
		CDebugBreakpointData *dataBreakpoint = (CDebugBreakpointData *)it->second;
		if (dataBreakpoint->isActive == false)
			continue;

		bool addrMatches;
		if (dataBreakpoint->IsRange())
		{
			addrMatches = (addr >= dataBreakpoint->addr && addr <= dataBreakpoint->addrEnd);
		}
		else
		{
			addrMatches = (addr == dataBreakpoint->addr);
		}
		if (!addrMatches)
			continue;

		// check memory access (read/write)
		if (!IS_SET(dataBreakpoint->dataAccess, memoryAccess))
			continue;

		if (!MemoryBreakpointValueMatches(value, dataBreakpoint->value, dataBreakpoint->comparison))
			continue;

		if (dataBreakpoint->callback)
		{
			if (dataBreakpoint->callback->DebugBreakpointEvaluateCallback(dataBreakpoint) == false)
				continue;
		}

		evaluateBreakpoint = dataBreakpoint;
		break;
	}

	// flag breakpoint to Server API
	if (evaluateBreakpoint && viewC64->debuggerServer)
	{
		if (viewC64->debuggerServer->AreClientsConnected())
		{
			nlohmann::json j;
			evaluateBreakpoint->GetDetailsJson(j);
			j["addr"] = addr;
			j["value"] = value;
			if (memoryAccess == MEMORY_BREAKPOINT_ACCESS_WRITE)
			{
				j["access"] = "write";
			}
			else if (memoryAccess == MEMORY_BREAKPOINT_ACCESS_READ)
			{
				j["access"] = "read";
			}
			viewC64->debuggerServer->BroadcastEvent("breakpoint", j);
		}
	}

	return evaluateBreakpoint;
}

void CDebugBreakpointsData::RenderImGui()
{
//	LOGD("CDebugBreakpointsData::RenderImGui");

	char *buf = SYS_GetCharBuf();

	ImVec4 colorNotActive(0.5, 0.5, 0.5, 1);
	ImVec4 colorActive(1.0, 1.0, 1.0, 1);

	sprintf(buf, "##BreakpointsDataTable_%s", symbols->dataAdapter->adapterID);

	symbols->LockMutex();

	CDebugBreakpointAddr *deleteBreakpoint = NULL;

	// active | address | end | <= | FF | symbol | delete
	if (ImGui::BeginTable(buf, 7, ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders))	//| ImGuiTableFlags_Sortable  TODO
	{
		u32 i = 0;

//		ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImVec4(0.7f, 0.7f, 0.7f, 0.65f)));

		ImGui::TableNextColumn();
		ImGui::Text("Active");
		ImGui::TableNextColumn();
		ImGui::Text("Address");
		ImGui::TableNextColumn();
		ImGui::Text("End");
		ImGui::TableNextColumn();
		ImGui::Text("Comparison");
		ImGui::TableNextColumn();
		ImGui::Text("Value");
		ImGui::TableNextColumn();
		ImGui::Text("Label");
		ImGui::TableNextColumn();
		ImGui::Text("");

//	   	ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, 0);

		for (std::map<int, CDebugBreakpointAddr *>::iterator it = breakpoints.begin(); it != breakpoints.end(); it++)
		{
			CDebugBreakpointData *bp = (CDebugBreakpointData*)it->second;

			sprintf(buf, "##chkBoxMem%x%d", this, i);

			ImGui::TableNextColumn();
			if (ImGui::Checkbox(buf, &bp->isActive))
			{
				// we do not need to update anything
			}

			ImGui::TableNextColumn();
			ImVec4 color = bp->isActive ? colorActive : colorNotActive;
			ImGui::TextColored(color, addressFormatStr, bp->addr);

			ImGui::TableNextColumn();
			if (bp->IsRange())
			{
				ImGui::TextColored(color, addressFormatStr, bp->addrEnd);
			}
			else
			{
				ImGui::TextColored(color, "");
			}

			ImGui::TableNextColumn();
			const char *comparisonStr = DataBreakpointComparisonToStr(bp->comparison);
			ImGui::TextColored(color, comparisonStr, bp->addr);

			ImGui::TableNextColumn();
			ImGui::TextColored(color, "%02X", bp->value);

			ImGui::TableNextColumn();
			CDebugSymbolsCodeLabel *label = NULL;
			if (symbols->currentSegment)
			{
				label = symbols->currentSegment->FindLabel(bp->addr);
			}
			ImGui::TextColored(color, label ? label->GetLabelText() : "");

			ImGui::TableNextColumn();
			sprintf(buf, "X##%x%d", this, i);
			if (ImGui::Button(buf))
			{
				// delete breakpoint
				deleteBreakpoint = bp;
			}

			i++;
		}

		ImGui::EndTable();
	}

	if (deleteBreakpoint)
	{
		DeleteBreakpoint(deleteBreakpoint);
		symbols->UpdateRenderBreakpoints();
	}

	if (ImGui::Button("+") || (ImGui::IsWindowFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)))
	{
		ImGui::OpenPopup("addMemBreakpointPopup");

		addBreakpointPopupAddr = 0;
		addBreakpointPopupSymbol[0] = '\0';
		addBreakpointPopupValue = 0xFF;
		addBreakpointPopupComparisonMethod = DataBreakpointComparison::MEMORY_BREAKPOINT_LESS_OR_EQUAL;
		addBreakpointPopupAddrEndBuf[0] = 0;
		comboFilterTextBuf[0] = 0;
		comboFilterState.activeIdx = 0;

		imGuiOpenPopupFrame = 0;
	}

	if (ImGui::BeginPopup("addMemBreakpointPopup"))
	{
		ImGui::Text(addBreakpointPopupHeadlineStr);
		ImGui::Separator();

		sprintf(buf, "##addMemBreakpointPopupAddress_%s", symbols->dataAdapter->adapterID);

		if (imGuiOpenPopupFrame < 2)
		{
			ImGui::SetKeyboardFocusHere();
			imGuiOpenPopupFrame++;
		}

		// Address input is free-form so labels can be typed. Enter mirrors PC
		// breakpoints: submit immediately; Tab/Shift+Tab still moves focus.
		ImGui::SetNextItemWidth(180.0f);
		bool addressInputActivated = ImGui::InputText(buf, comboFilterTextBuf, IM_ARRAYSIZE(comboFilterTextBuf),
											 ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::SameLine();

		// "End" address — optional. Empty (or value <= start) means single-address.
		ImGui::Text("..");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		sprintf(buf, "##addMemBreakpointPopupEndAddr%x", this);
		bool endInputActivated = ImGui::InputText(buf, addBreakpointPopupAddrEndBuf, IM_ARRAYSIZE(addBreakpointPopupAddrEndBuf),
									  ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_EnterReturnsTrue);

		ImGui::SameLine();

		//
		const char *selectedComparisonStr = DataBreakpointComparisonToStr((DataBreakpointComparison)addBreakpointPopupComparisonMethod);

		sprintf(buf, "##addMemBreakpointPopupComboComparison%x", this);
		if (ImGui::BeginCombo(buf, selectedComparisonStr))
		{
			for (int n = 0; n < MEMORY_BREAKPOINT_ARRAY_SIZE; n++)
			{
				bool is_selected = (addBreakpointPopupComparisonMethod == n);
				if (ImGui::Selectable(comparisonMethodsStr[n], is_selected))
				{
					addBreakpointPopupComparisonMethod = n;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();

		sprintf(buf, "##addMemBreakpointPopupValue%x", this);
		ImGui::InputScalar(buf, ImGuiDataType_::ImGuiDataType_U8, &addBreakpointPopupValue, NULL, NULL, "%02X",
						   ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_CharsUppercase);

		// Filtered hint list (memory breakpoints have labels). Mirrors the
		// pattern used in CViewDataWatch / CDebugBreakpointsAddr.
		const char **hints = NULL;
		int numHints = 0;
		if (symbols->currentSegment)
		{
			hints = symbols->currentSegment->codeLabelsArray;
			numHints = symbols->currentSegment->numCodeLabelsInArray;
		}

		const bool filterIsHex = (comboFilterTextBuf[0] != 0) && FUN_IsHexNumber(comboFilterTextBuf);
		char needleLower[MAX_LABEL_TEXT_BUFFER_SIZE];
		int nlen = 0;
		for (int i = 0; comboFilterTextBuf[i] != 0 && i < (int)sizeof(needleLower) - 1; i++)
		{
			char c = comboFilterTextBuf[i];
			needleLower[nlen++] = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
		}
		needleLower[nlen] = 0;

		const int kMaxMatches = 512;
		int matchIdx[kMaxMatches];
		int matchCount = 0;
		if (hints != NULL && numHints > 0 && !filterIsHex)
		{
			for (int n = 0; n < numHints && matchCount < kMaxMatches; n++)
			{
				const char *hay = hints[n];
				if (hay == NULL) continue;

				bool match = true;
				if (nlen > 0)
				{
					match = false;
					for (const char *p = hay; *p && !match; p++)
					{
						const char *h = p;
						const char *q = needleLower;
						while (*h && *q)
						{
							char hc = *h;
							if (hc >= 'A' && hc <= 'Z') hc += 32;
							if (hc != *q) break;
							h++; q++;
						}
						if (*q == 0) match = true;
					}
				}
				if (match) matchIdx[matchCount++] = n;
			}
		}

		if (comboFilterState.activeIdx >= matchCount) comboFilterState.activeIdx = matchCount > 0 ? matchCount - 1 : 0;
		if (comboFilterState.activeIdx < 0) comboFilterState.activeIdx = 0;

		bool arrowScrolled = false;
		if (matchCount > 0)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			{
				if (comboFilterState.activeIdx < matchCount - 1) comboFilterState.activeIdx++;
				arrowScrolled = true;
			}
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			{
				if (comboFilterState.activeIdx > 0) comboFilterState.activeIdx--;
				arrowScrolled = true;
			}
		}

		// Picking from the hint list copies the label text into the address
		// input but does NOT submit — user still needs Create Breakpoint or Enter
		// in an address field so they can finish adjusting comparison/value.
		if (hints != NULL && numHints > 0)
		{
			ImGui::BeginChild("##memBpHints", ImVec2(280.0f, 160.0f), true);
			for (int i = 0; i < matchCount; i++)
			{
				int n = matchIdx[i];
				const char *hay = hints[n];
				const bool is_selected = (i == comboFilterState.activeIdx);
				ImGui::PushID(n);
				if (ImGui::Selectable(hay, is_selected))
				{
					strcpy(comboFilterTextBuf, hay);
					comboFilterState.activeIdx = i;
				}
				if (is_selected && arrowScrolled)
				{
					ImGui::SetScrollHereY();
				}
				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		// Enter on a highlighted match commits that label before finalizing.
		if (addressInputActivated && matchCount > 0 && !filterIsHex
			&& comboFilterState.activeIdx >= 0 && comboFilterState.activeIdx < matchCount)
		{
			strcpy(comboFilterTextBuf, hints[matchIdx[comboFilterState.activeIdx]]);
		}

		bool finalizeAddingBreakpoint = addressInputActivated || endInputActivated;

		bool createButtonClicked = ImGui::Button("Create Breakpoint");
		bool createButtonActivated = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter);
		if (createButtonClicked || createButtonActivated)
		{
			finalizeAddingBreakpoint = true;
		}

		if (finalizeAddingBreakpoint)
		{
			CDebugSymbolsCodeLabel *label = symbols->currentSegment->FindLabelByText(comboFilterTextBuf);
			if (label)
			{
				addBreakpointPopupAddr = label->address;
			}
			else if (FUN_IsHexNumber(comboFilterTextBuf))
			{
				FUN_ToUpperCaseStr(comboFilterTextBuf);
				addBreakpointPopupAddr = FUN_HexStrToValue(comboFilterTextBuf);
			}
			else
			{
				char *buf = SYS_GetCharBuf();
				sprintf(buf, "Invalid address or symbol:\n%s", comboFilterTextBuf);
				guiMain->ShowMessageBox("Can't add breakpoint", buf);
				SYS_ReleaseCharBuf(buf);
				addBreakpointPopupAddr = -1;
			}

			// Parse optional "End" address. Blank input -> single address.
			int addrEnd = -1;
			if (addBreakpointPopupAddrEndBuf[0] != 0)
			{
				if (FUN_IsHexNumber(addBreakpointPopupAddrEndBuf))
				{
					FUN_ToUpperCaseStr(addBreakpointPopupAddrEndBuf);
					addrEnd = FUN_HexStrToValue(addBreakpointPopupAddrEndBuf);
				}
				else
				{
					char *buf2 = SYS_GetCharBuf();
					sprintf(buf2, "Invalid end address:\n%s", addBreakpointPopupAddrEndBuf);
					guiMain->ShowMessageBox("Can't add breakpoint", buf2);
					SYS_ReleaseCharBuf(buf2);
					addBreakpointPopupAddr = -1;
				}
			}

			if (addBreakpointPopupAddr >= 0)
			{
				addBreakpointPopupAddr = URANGE(minAddr, addBreakpointPopupAddr, maxAddr);
				addBreakpointPopupValue = URANGE(0, addBreakpointPopupValue, 0xFF);

				if (addrEnd >= 0)
				{
					addrEnd = URANGE(minAddr, addrEnd, maxAddr);
					// If end <= start, fall back to single-address mode rather than
					// erroring — keeps the popup forgiving.
					if (addrEnd <= addBreakpointPopupAddr)
					{
						addrEnd = -1;
					}
				}

				CDebugBreakpointData *breakpoint = new CDebugBreakpointData(symbols, addBreakpointPopupAddr, addrEnd,
																	  MEMORY_BREAKPOINT_ACCESS_WRITE,
																	  (DataBreakpointComparison)addBreakpointPopupComparisonMethod,
																	  addBreakpointPopupValue);
				AddBreakpoint(breakpoint);
				UpdateRenderBreakpoints();

				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndPopup();
	}

	symbols->UnlockMutex();

	SYS_ReleaseCharBuf(buf);
}

const char *CDebugBreakpointsData::DataBreakpointComparisonToStr(DataBreakpointComparison comparison)
{
	switch(comparison)
	{
		case DataBreakpointComparison::MEMORY_BREAKPOINT_EQUAL:
			return "==";
		case DataBreakpointComparison::MEMORY_BREAKPOINT_NOT_EQUAL:
			return "!=";
		case DataBreakpointComparison::MEMORY_BREAKPOINT_LESS:
			return "<";
		case DataBreakpointComparison::MEMORY_BREAKPOINT_LESS_OR_EQUAL:
			return "<=";
		case DataBreakpointComparison::MEMORY_BREAKPOINT_GREATER:
			return ">";
		case DataBreakpointComparison::MEMORY_BREAKPOINT_GREATER_OR_EQUAL:
			return ">=";
		default:
			return "???";
	}
}

DataBreakpointComparison CDebugBreakpointsData::StrToDataBreakpointComparison(const char *comparisonStr)
{
	for (int i = 0; i < MEMORY_BREAKPOINT_ARRAY_SIZE; i++)
	{
		if (!strcmp(comparisonStr, comparisonMethodsStr[i]))
		{
			return DataBreakpointComparison(i);
		}
	}

	return (DataBreakpointComparison)MEMORY_BREAKPOINT_ARRAY_SIZE;
}


CDebugBreakpointsData::CDebugBreakpointsData(int breakpointType, const char *breakpointTypeStr, CDebugSymbolsSegment *segment, const char *addressFormatStr, int minAddr, int maxAddr)
: CDebugBreakpointsAddr(breakpointType, breakpointTypeStr, segment, addressFormatStr, minAddr, maxAddr)
{
	addBreakpointPopupHeadlineStr = "Add Memory Breakpoint";
	addBreakpointPopupAddrStr = "Address";
	addBreakpointPopupAddrEndBuf[0] = 0;
}

CDebugBreakpoint *CDebugBreakpointsData::CreateEmptyBreakpoint()
{
	return new CDebugBreakpointData(symbols, 0, -1, 0, DataBreakpointComparison::MEMORY_BREAKPOINT_EQUAL, 0);
}

CDebugBreakpointsData::~CDebugBreakpointsData()
{
	ClearBreakpoints();
}
