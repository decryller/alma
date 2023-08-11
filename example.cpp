#include "alma.hpp"
#include "rvmt/rvmt.hpp"
#include <cstdio>
#include <iostream>
#include <thread>

int main() {
	RVMT::Start();
	bool quit = false;

	enum GUIPage {
		GUIPage_LANDING				= 0,
		GUIPage_SCANNINGRESULTS		= 1,
		GUIPage_SCANNINGSETTINGS	= 2,
		GUIPage_MEMORYREAD          = 3,
		GUIPage_MEMORYWRITE			= 4
	};

	GUIPage GUIPage_CURRENT = GUIPage_LANDING;

	bool memInspectionRequested = false;

	std::thread autoUpdateInspection([&quit, &memInspectionRequested, &GUIPage_CURRENT](){
		while (!quit) {
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (GUIPage_CURRENT == GUIPage_MEMORYREAD) {
				RVMT::renderRequests.push_back(1);
				memInspectionRequested = true;
			}
		}
	});

	while (!quit) {

		while (RVMT::renderRequests.size() == 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(16));

		// Consume render request.
		RVMT::renderRequests.erase(RVMT::renderRequests.begin());

		const unsigned short rowCount = RVMT::internal::rowCount;
		const unsigned short colCount = RVMT::internal::colCount;

		RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 1);
		RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 1);

		switch (GUIPage_CURRENT) {
			// These variables are shared between GUI Pages.
			static int includedPermissions =
				memoryPermission_READ |
				memoryPermission_WRITE |
				memoryPermission_PRIVATE |
				memoryPermission_SHARED;

			static int excludedPermissions = memoryPermission_EXECUTE;

			static char scanAlignmentInput[5] = "4";
			static char maxOccurrencesInput[5] = "0";
			static char memoryReadAddrInput[33];

			case GUIPage_LANDING:
				RVMT::Text("Waiting for a process to be opened");
				break;

			case GUIPage_SCANNINGRESULTS:
				static char patternInput[129];
				static std::vector<memAddr> scanResults;

				RVMT::Text("Pattern scanning ");

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::PushPropertyForNextItem(WidgetProp_InputText_IdleText, "Input (Use ?? for wildcards)");
				RVMT::InputText("scanPatternInput field", patternInput, 128, 32);

				RVMT::SameLine();
				if (RVMT::Button("Search")) {

					std::vector<unsigned short> pattern;
					std::istringstream iss(patternInput);
					std::string parsedByte;

					while (std::getline(iss, parsedByte, ' ')) 
						pattern.push_back (
							parsedByte.find('?') == std::string::npos
							? std::stoi(parsedByte, nullptr, 16)
							: 0x420
						);

					scanResults.clear();
					const int targetOccurrences = std::stoi(maxOccurrencesInput);

					for (auto &page : alma::getMemoryPages(includedPermissions, excludedPermissions)) {
						const unsigned int maxOccurrences = targetOccurrences == 0 ? 0 : targetOccurrences - scanResults.size();
						const auto result = alma::patternScan(page.begin, page.end, pattern, std::stoi(scanAlignmentInput), maxOccurrences);
						
						if (result[0] != 0xBAD) {
							if (targetOccurrences == 0 || scanResults.size() + result.size() <= targetOccurrences)
								scanResults.insert(scanResults.end(), result.begin(), result.end());
							
							else { // Target occurrences have been reached.
								scanResults.insert(scanResults.end(), result.begin(), result.begin() + scanResults.size() - targetOccurrences);
								break;
							}
						}
					}
				}

				if (scanResults.size() > 0) {
					RVMT::SetCursorY(NewCursorPos_ADD, 1);
					RVMT::Text("Click an address to check its memory");

					unsigned int addressColumns = 1;
					for (int i = 0; i < scanResults.size(); i++) {
						RVMT::PushPropertyForNextItem(WidgetProp_Button_TextOnly);

						if (RVMT::Button("%llX", scanResults[i])) {
							snprintf(memoryReadAddrInput, 32, "%llX", scanResults[i]);
							memInspectionRequested = true;
							GUIPage_CURRENT = GUIPage_MEMORYREAD;
						}

						if (i % (rowCount - 7) == rowCount - 8) {
							RVMT::SetCursorX(NewCursorPos_ADD, 13);
							RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 4);
						}
					}
				}

				RVMT::SetCursorX(NewCursorPos_ABSOLUTE, colCount - 13);
				RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 0);
				if (RVMT::Button(" Settings ")) 
					GUIPage_CURRENT = GUIPage_SCANNINGSETTINGS;

				break;
			
			case GUIPage_SCANNINGSETTINGS:
				// These bools just change the appearance of the checkboxes.
				static bool includePermission[4] {1,0,1,1};

				RVMT::Text("Alignment: ");

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::InputText("alignment input", scanAlignmentInput, 4, 4);

				RVMT::SetCursorY(NewCursorPos_ADD, 2);
				RVMT::Text("Max ocurrences: ");

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::InputText("maxOcurrencesInput field", maxOccurrencesInput, 4, 4);

				RVMT::SetCursorY(NewCursorPos_ADD, 1);
				RVMT::Text("Permission filtering: ");

				RVMT::Text("memoryPermission_WRITE ");
				RVMT::SameLine();
				if (RVMT::Checkbox("[Include]", "[Exclude]", &includePermission[0])) 
					includedPermissions ^= memoryPermission_WRITE,
					excludedPermissions ^= memoryPermission_WRITE;

				RVMT::Text("memoryPermission_EXECUTE ");
				RVMT::SameLine();
				if (RVMT::Checkbox("[Include]", "[Exclude]", &includePermission[1])) 
					includedPermissions ^= memoryPermission_EXECUTE,
					excludedPermissions ^= memoryPermission_EXECUTE;

				RVMT::Text("memoryPermission_PRIVATE ");
				RVMT::SameLine();
				if (RVMT::Checkbox("[Include]", "[Exclude]", &includePermission[2])) 
					includedPermissions ^= memoryPermission_PRIVATE,
					excludedPermissions ^= memoryPermission_PRIVATE;

				RVMT::Text("memoryPermission_SHARED ");
				RVMT::SameLine();
				if (RVMT::Checkbox("[Include]", "[Exclude]", &includePermission[3])) 
					includedPermissions ^= memoryPermission_SHARED,
					excludedPermissions ^= memoryPermission_SHARED;

				RVMT::SetCursorX(NewCursorPos_ABSOLUTE, colCount - 12);
				RVMT::SetCursorY(NewCursorPos_ABSOLUTE, 0);
				if (RVMT::Button(" Results ")) 
					GUIPage_CURRENT = GUIPage_SCANNINGRESULTS;

				break;

			case GUIPage_MEMORYREAD:
				static char readCountInput[6] = "512";
				static char addrPerRowInput[6] = "16";
				static unsigned int addressesPerRow = 16;

				static std::vector<unsigned char> readingResults;

				RVMT::PushPropertyForNextItem(WidgetProp_InputText_CustomCharset, "abcdefABCDEF1234567890");
				RVMT::InputText("memoryReadAddrInput field ", memoryReadAddrInput, 32, 16);

				RVMT::SameLine();

				if (RVMT::Button("Inspect") || memInspectionRequested && memoryReadAddrInput[0] != 0 && readCountInput[0] != 0) {
					readingResults = alma::memRead(std::strtoull(memoryReadAddrInput, 0, 16), std::stoi(readCountInput));
					addressesPerRow = std::stoi(addrPerRowInput);
					memInspectionRequested = false;
				}
				
				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_ADD, 1);
				RVMT::Text(" Addresses to scan: ");

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::PushPropertyForNextItem(WidgetProp_InputText_CustomCharset, "abcdefABCDEF1234567890");
				RVMT::InputText("readCountInput field", readCountInput, 5, 5);

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_ADD, 1);
				RVMT::Text(" Addresses per row: ");

				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::PushPropertyForNextItem(WidgetProp_InputText_CustomCharset, "1234567890");
				RVMT::InputText("addrPerRowInput field", addrPerRowInput, 5, 5);

				RVMT::SameLine();
				if (RVMT::Button("+128")) {
					snprintf(memoryReadAddrInput, 32, "%llX", std::strtoull(memoryReadAddrInput, 0, 16) + 128);
					memInspectionRequested = true;
				}

				RVMT::SameLine();
				if (RVMT::Button("+256")) {
					snprintf(memoryReadAddrInput, 32, "%llX", std::strtoull(memoryReadAddrInput, 0, 16) + 256);
					memInspectionRequested = true;
				}

				for (int i = 0; i < readingResults.size(); i++) {

					if (i % addressesPerRow == 0) {
						if (i / addressesPerRow == rowCount - 7)
							break;
						RVMT::Text("+%04lX | ", i);
						RVMT::SameLine();
					}

					RVMT::Text("%02X ", readingResults[i]);

					if (i % addressesPerRow != addressesPerRow - 1)
						RVMT::SameLine();
				}
				break;

			case GUIPage_MEMORYWRITE:
				static char addressInput[33] = "\0";
				static char pattternInput[129] = "\0";

				RVMT::Text("Address to write to: ");
				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::InputText("memory write address input", addressInput, 32, 16);

				RVMT::SetCursorY(NewCursorPos_ADD, 2);
				RVMT::Text("Bytes to write: ");
				RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
				RVMT::InputText("memory write pattern input", pattternInput, 128, 32);

				RVMT::SetCursorY(NewCursorPos_ADD, 2);
				if (RVMT::Button("Write")) {
					std::vector<unsigned char> pattern;
					std::istringstream iss(pattternInput);
					std::string parsedByte;

					while (std::getline(iss, parsedByte, ' ')) 
						pattern.push_back(std::stoi(parsedByte, nullptr, 16));
					
					alma::memWrite(std::strtoull(addressInput, 0, 16), pattern);
				}
				break;
			default:
				RVMT::Text("unknown gui page...");
				break;
		}

		// === Bottom bar
		static std::string pidInput(16, 0);

		RVMT::SetCursorX(NewCursorPos_ABSOLUTE, 1);
		RVMT::SetCursorY(NewCursorPos_ABSOLUTE, rowCount - 2);

		RVMT::DrawBox(0, rowCount - 4, colCount - 2, 4);
		RVMT::Text("PID: ");

		RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
		RVMT::PushPropertyForNextItem(WidgetProp_InputText_CustomCharset, "1234567890");
		RVMT::InputText("pid input", &pidInput[0], 16, 6);

		RVMT::SameLine();
		if (RVMT::Button("Open")) 
			if (alma::openProcess(std::stoi(pidInput)))
				GUIPage_CURRENT = GUIPage_SCANNINGRESULTS;
		
		RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_ADD, 1);
		RVMT::Text(" Current process' PID: %i ", alma::getCurrentlyOpenPID());

		if (alma::getCurrentlyOpenPID() > 0) {
			RVMT::SameLine(); RVMT::SetCursorY(NewCursorPos_SUBTRACT, 1);
			if (RVMT::Button("Pattern scanning"))
				GUIPage_CURRENT = GUIPage_SCANNINGRESULTS;

			RVMT::SameLine(); 
			if (RVMT::Button("Memory reading"))
				GUIPage_CURRENT = GUIPage_MEMORYREAD;

			RVMT::SameLine(); 
			if (RVMT::Button("Memory writing"))
				GUIPage_CURRENT = GUIPage_MEMORYWRITE;
		}

		RVMT::SetCursorY(NewCursorPos_ABSOLUTE, rowCount - 3);
		RVMT::SetCursorX(NewCursorPos_ABSOLUTE, colCount - 9);

		if (RVMT::Button(" Quit "))
			quit = true;
		RVMT::Render();
	}

	autoUpdateInspection.join();
	std::wcout << "\nExited main loop."; std::wcout.flush();
	RVMT::Stop();

	return 0;
}