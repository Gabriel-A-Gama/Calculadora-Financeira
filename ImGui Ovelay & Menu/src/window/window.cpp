#include "window.hpp"
#include "math.h"
// painnnnn
#include "../../ext/ImGui 1.90/imgui.h"
#include "../../ext/ImGui 1.90/imgui_impl_dx11.h"
#include "../../ext/ImGui 1.90/imgui_impl_win32.h"

#include <dwmapi.h>
#include <stdio.h>

ID3D11Device* Overlay::device = nullptr;

// sends rendering commands to the device
ID3D11DeviceContext* Overlay::device_context = nullptr;

// manages the buffers for rendering, also presents rendered frames.
IDXGISwapChain* Overlay::swap_chain = nullptr;

// represents the target surface for rendering
ID3D11RenderTargetView* Overlay::render_targetview = nullptr;

HWND Overlay::overlay = nullptr;
WNDCLASSEX Overlay::wc = { };

// declaration of the ImGui_ImplWin32_WndProcHandler function
// basically integrates ImGui with the Windows message loop so ImGui can process input and events
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK window_procedure(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// set up ImGui window procedure handler
	if (ImGui_ImplWin32_WndProcHandler(window, msg, wParam, lParam))
		return true;

	// switch that disables alt application and checks for if the user tries to close the window.
	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu (imgui uses it in their example :shrug:)
			return 0;
		break;

	case WM_DESTROY:
		Overlay::DestroyDevice();
		Overlay::DestroyOverlay();
		Overlay::DestroyImGui();
		PostQuitMessage(0);
		return 0;

	case WM_CLOSE:
		Overlay::DestroyDevice();
		Overlay::DestroyOverlay();
		Overlay::DestroyImGui();
		return 0;
	}

	// define the window procedure
	return DefWindowProc(window, msg, wParam, lParam);
}

bool Overlay::CreateDevice()
{
	// First we setup our swap chain, this basically just holds a bunch of descriptors for the swap chain.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));

	// set number of back buffers (this is double buffering)
	sd.BufferCount = 2;

	// width + height of buffer, (0 is automatic sizing)
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;

	// set the pixel format
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// set the fps of the buffer (60 at the moment)
	sd.BufferDesc.RefreshRate.Numerator = 60; 
	sd.BufferDesc.RefreshRate.Denominator = 1;

	// allow mode switch (changing display modes)
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// set how the bbuffer will be used
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

	sd.OutputWindow = overlay;

	// setup the multi-sampling
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;

	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	// specify what Direct3D feature levels to use
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	
	// create device and swap chain
	HRESULT result = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&featureLevel,
		&device_context);

	// if the hardware isn't supported create with WARP (basically just a different renderer)
	if (result == DXGI_ERROR_UNSUPPORTED) {
		result = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			0U,
			featureLevelArray,
			2, D3D11_SDK_VERSION,
			&sd,
			&swap_chain,
			&device,
			&featureLevel,
			&device_context);

		printf("[>>] DXGI_ERROR | Created with D3D_DRIVER_TYPE_WARP\n");
	}

	// can't do much more, if the hardware still isn't supported just return false.
	if (result != S_OK) {
		printf("Device Not Okay\n");
		return false;
	}

	// retrieve back_buffer, im defining it here since it isn't being used at any other point in time.
	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	// if back buffer is obtained then we can create render target view and release the back buffer again
	if (back_buffer) 
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_targetview);
		back_buffer->Release();

		printf("[>>] Created Device\n");
		return true;
	}

	// if we reach this point then it failed to create the back buffer
	printf("[>>] Failed to create Device\n");
	return false;
}

void Overlay::DestroyDevice()
{
	// release everything that has to do with the device.
	if (device)
	{
		device->Release();
		device_context->Release();
		swap_chain->Release();
		render_targetview->Release();

		printf("[>>] Released Device\n");
	}
	else
		printf("[>>] Device Not Found when Exiting.\n");
}

void Overlay::CreateOverlay()
{
	// holds descriptors for the window, called a WindowClass
	// set up window class
	wc.cbSize = sizeof(wc);
	wc.style = CS_CLASSDC;
	wc.lpfnWndProc = window_procedure;
	wc.hInstance = GetModuleHandleA(0);
	wc.lpszClassName = "MicroSoft Edge";

	// register our class
	RegisterClassEx(&wc);

	// create window (the actual one that shows up in your taskbar)
	// WS_EX_TOOLWINDOW hides the new window that shows up in your taskbar and attaches it to any already existing windows instead.
	// (in this case the console)
	overlay = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		wc.lpszClassName,
		"Microsoft",
		WS_POPUP,
		0,
		0,
		GetSystemMetrics(SM_CXSCREEN), // 1920
		GetSystemMetrics(SM_CYSCREEN), // 1080
		NULL,
		NULL,
		wc.hInstance,
		NULL
	);

	if (overlay == NULL)
		printf("Failed to create Overlay\n");

	// set overlay window attributes to make the overlay transparent
	SetLayeredWindowAttributes(overlay, RGB(0, 0, 0), BYTE(255), LWA_ALPHA);

	// set up the DWM frame extension for client area
	{
		// first we define our RECT structures that hold our client and window area
		RECT client_area{};
		RECT window_area{};

		// get the client and window area
		GetClientRect(overlay, &client_area);
		GetWindowRect(overlay, &window_area);

		// calculate the difference between the screen and window coordinates
		POINT diff{};
		ClientToScreen(overlay, &diff);

		// calculate the margins for DWM frame extension
		const MARGINS margins{
			window_area.left + (diff.x - window_area.left),
			window_area.top + (diff.y - window_area.top),
			client_area.right,
			client_area.bottom
		};

		// then we extend the frame into the client area
		DwmExtendFrameIntoClientArea(overlay, &margins);
	}

	// show + update overlay
	ShowWindow(overlay, SW_SHOW);
	UpdateWindow(overlay);

	printf("[>>] Overlay Created\n");
}

void Overlay::DestroyOverlay()
{
	DestroyWindow(overlay);
	UnregisterClass(wc.lpszClassName, wc.hInstance);
}

bool Overlay::CreateImGui()
{
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	// Initalize ImGui for the Win32 library
	if (!ImGui_ImplWin32_Init(overlay)) {
		printf("Failed ImGui_ImplWin32_Init\n");
		return false;
	}
	
	// Initalize ImGui for DirectX 11.
	if (!ImGui_ImplDX11_Init(device, device_context)) {
		printf("Failed ImGui_ImplDX11_Init\n");
		return false;
	}

	printf("[>>] ImGui Initialized\n");
	return true;
}

void Overlay::DestroyImGui()
{
	// Cleanup ImGui by shutting down DirectX11, the Win32 Platform and Destroying the ImGui context.
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void Overlay::StartRender()
{
	// handle windows messages
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// begin a new frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
	// if the user presses Insert then enable the menu.
	if (GetAsyncKeyState(VK_INSERT) & 1) {
		RenderMenu = !RenderMenu;

		// If we are rendering the menu set the window styles to be able to clicked on.
		if (RenderMenu) {
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
		}
		else {
			SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_LAYERED);
			//SetWindowLong(overlay, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST);
		}
	}
}

void Overlay::EndRender()
{
	// Render ImGui
	ImGui::Render();

	// Make a color that's clear / transparent
	float color[4]{ 0, 0, 0, 0 };

	// Set the render target and then clear it
	device_context->OMSetRenderTargets(1, &render_targetview, nullptr);
	device_context->ClearRenderTargetView(render_targetview, color);

	// Render ImGui draw data.
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// Present rendered frame with V-Sync
	swap_chain->Present(1U, 0U);

	// Present rendered frame without V-Sync
	//swap_chain->Present(0U, 0U);
}

void menustyle() {
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.60f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(15.00f, 15.00f);
	style.FramePadding = ImVec2(8.00f, 5.00f);
	style.ItemSpacing = ImVec2(10.00f, 5.00f);
	style.ItemInnerSpacing = ImVec2(8.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25.00f;
	style.ScrollbarSize = 15.00f;
	style.GrabMinSize = 8.00f;
	style.WindowBorderSize = 1.00f;
	style.ChildBorderSize = 1.00f;
	style.PopupBorderSize = 1.00f;
	style.FrameBorderSize = 0.00f;
	style.TabBorderSize = 1.00f;
	style.WindowRounding = 5.00f;
	style.ChildRounding = 5.00f;
	style.FrameRounding = 2.00f;
	style.PopupRounding = 5.00f;
	style.ScrollbarRounding = 9.00f;
	style.GrabRounding = 3.00f;
	style.TabRounding = 4.00f;
}

double converte_taxa(int verifica_taxa, double taxa) {
	if (verifica_taxa == 1) { // converte para dia
		taxa = taxa / 30.00;
		return taxa;
	}
	else if (verifica_taxa == 3) { // converte para ano
		taxa = taxa / 12.00;
		return taxa;
	}
	return taxa;
}

int converte_prazo(int verifica_prazo, float prazo) {
	if (verifica_prazo == 1) { // converte de dia para mês
		prazo = prazo / 30;
		return prazo;
	}
	else if (verifica_prazo == 3) { // converte de ano para mês
		prazo = prazo * 12;
		return prazo;
	}
	return prazo;
}


void renderizar_interface() {
	static int escolha = 0;
	static float taxa = 0.0f;
	static int v_taxa = 1;
	static float prazo = 0.0f;
	static int v_prazo = 1;
	static float vp = 0.0f;
	static float juros = 0.0f;
	static float vf = 0.0f; 
	static int op_desconto = 0; 
	static float dr_composto = 0.0f; 
	static float dc_composto = 0.0f;
	static float dr_simples = 0.0f;
	static float dc_simples = 0.0f;
	ImGui::Begin("Calculadora Financeira");
	//ImGui::Begin("cheat", &RenderMenu, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

	ImGui::Text("Escolha uma opcao:");
	ImGui::RadioButton("Calcular Juros", &escolha, 0);
	ImGui::RadioButton("Calcular Prazo", &escolha, 1);
	ImGui::RadioButton("Calcular Taxa", &escolha, 2);
	ImGui::RadioButton("Calcular Valor Futuro", &escolha, 3);
	ImGui::RadioButton("Calcular Valor Presente", &escolha, 4);
	ImGui::RadioButton("Calcular Descontos Racional", &escolha, 5);
	ImGui::RadioButton("Calcular Descontos Comercial", &escolha, 6);
	switch (escolha) {
	case 0: // Calcular Juros
	
		ImGui::Separator();
		ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");
		ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");
		ImGui::InputFloat("Valor Presente", &vp, 0.0f, 0.0f, "%.1f");

		if (ImGui::Button("Calcular")) {
			if (v_taxa != 2)
				prazo = converte_prazo(v_prazo, prazo);

			if (v_prazo != 2)
				taxa = converte_taxa(v_taxa, taxa);

			taxa = taxa / 100.00;
			juros = vp * taxa * prazo;
		}

		ImGui::Text("Juros: R$%.2f", juros);

		break;

	case 1: // Calcular Prazo
	
		ImGui::Separator();
		ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");
		ImGui::InputFloat("Valor Presente", &vp, 0.0f, 0.0f, "%.1f");
		ImGui::InputFloat("Valor Futuro", &vf, 0.0f, 0.0f, "%.1f");

		if (ImGui::Button("Calcular")) {
			if (v_taxa == 2) {
				taxa = taxa / 100.00;
				prazo = ((vf / vp) - 1.00) / taxa;
			}

			else {
				if (v_taxa != 2)
					taxa = converte_taxa(v_taxa, taxa);

				taxa = taxa / 100.00;
				prazo = ((vf / vp) - 1.00) / taxa;
			}
		}

		ImGui::Text("Prazo: %.2f meses", prazo);
		break;

	case 2: // Calcular Taxa
	
		ImGui::Separator();
		ImGui::InputFloat("Valor Presente", &vp, 0.0f, 0.0f, "%.1f");
		ImGui::InputFloat("Valor Futuro", &vf, 0.0f, 0.0f, "%.1f");
		ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");

		if (ImGui::Button("Calcular")) {
			if (v_prazo == 2)
				taxa = ((vf / vp) - 1) / prazo;

			else {
				converte_prazo(v_prazo, prazo);
			}

			taxa = ((vf / vp) - 1) / prazo;
		}

		ImGui::Text("Taxa: %.2f %%", taxa * 100);
		break;

	case 3: // Calcular Valor Futuro
		ImGui::Separator();
		ImGui::InputFloat("Valor Presente", &vp, 0.0f, 0.0f, "%.1f");
		ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");
		ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");

		if (ImGui::Button("Calcular")) {
			if (v_prazo != 2)
				prazo = converte_prazo(v_prazo, prazo);

			if (v_taxa != 2)
				taxa = converte_taxa(v_prazo, taxa);

			taxa = taxa / 100.00;
			vf = vp * (1.00 + taxa * prazo);
		}

		ImGui::Text("Valor Futuro: R$%.2f", vf);
		break;


	case 4: // Calcular Valor Presente

		ImGui::Separator();
		ImGui::InputFloat("Valor Futuro", &vf, 0.0f, 0.0f, "%.1f");
		ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");
		ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
		ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");

		if (ImGui::Button("Calcular")) {
			if (v_prazo != 2)
				prazo = converte_prazo(v_prazo, prazo);

			if (v_taxa != 2)
				taxa = converte_taxa(v_taxa, taxa);

			taxa = taxa / 100.00;
			vp = vf / (1.00 + taxa * prazo);
		}

		ImGui::Text("Valor Presente: R$%.2f", vp);
		break;


	case 5: // Calcular Descontos

		ImGui::Separator();
		ImGui::Text("Calcular Descontos Racional");
		
	
			ImGui::InputFloat("Valor Futuro (VF)", &vf, 0.0f, 0.0f, "%.1f");
			ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
			ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");
			ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
			ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");

			if (ImGui::Button("Calcular")) {
				if (v_taxa != 2)
					prazo = converte_prazo(v_prazo, prazo);

				if (v_prazo != 2)
					taxa = converte_taxa(v_taxa, taxa);

				taxa = taxa / 100.00;
				dr_simples = vp * taxa * prazo;

				
			}
			ImGui::Text("Desconto Racional Simples: R$%.2f", dr_simples);
			break;
		
	case 6:
			ImGui::Separator();
			ImGui::Text("Calcular Descontos Comercial");
			ImGui::InputFloat("Valor Futuro (VF)", &vf, 0.0f, 0.0f, "%.1f");
			ImGui::InputFloat("Prazo", &prazo, 0.0f, 0.0f, "%.1f");
			
			ImGui::Combo("Tipo de Prazo", &v_prazo, "Dias\0Meses\0Anos\0");
			ImGui::InputFloat("Taxa (%)", &taxa, 0.0f, 0.0f, "%.1f");
			ImGui::Combo("Tipo de Taxa", &v_taxa, "Dias\0Meses\0Anos\0");

			if (ImGui::Button("Calcular")) {
				if (v_taxa != 2)
					taxa = converte_taxa(v_taxa, taxa);

				if (v_prazo != 2)
					prazo = converte_prazo(v_prazo, prazo);

				taxa = taxa / 100.00;
				dc_simples = vf * taxa * prazo;

				//ImGui::Text("Desconto Comercial Simples: R$%.2f", dc_simples);
			}
			ImGui::Text("Desconto Comercial Simples: R$%.2f", dc_simples);

		
		break;


	

	}
	

	if (ImGui::Button("Sair")) {
		exit(0);
	}
	ImGui::End();
}

void Overlay::Render()
{
	menustyle();
	ImGui::SetNextWindowSize({ 450, 500 });
	//ImGui::Begin("cheat", &RenderMenu, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
	renderizar_interface();


	//ImGui::End();
}

void Overlay::SetForeground(HWND window)
{
	if (!IsWindowInForeground(window))
		BringToForeground(window);
}