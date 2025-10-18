## Instalar vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

## Instalar dependências
.\vcpkg install glfw3:x64-windows
.\vcpkg install opencascade:x64-windows

## Integrar com Visual Studio
.\vcpkg integrate install


cmake .. -DCMAKE_TOOLCHAIN_FILE= --> PATH DO VCPKG <-- /scripts/buildsystems/vcpkg.cmake
cmake --build "C:/Users/Daniel/repos/lumi/build" --config Release -- /m


# Passo 3: Configurar e Compilar
powershell# Voltar para o diretório raiz do projeto
cd ..

## Compilar
cmake --build . --config Release

## Ou abrir no Visual Studio
start ImGuiOCCTApp.sln


sudo apt update
sudo apt install -y cmake build-essential git
sudo apt install -y libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
sudo apt install -y libocct-*-dev  # OpenCASCADE

