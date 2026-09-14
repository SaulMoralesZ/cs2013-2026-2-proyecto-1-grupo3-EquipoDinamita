# EquipoDinamita - Proyecto CS2013_2026-2

## 🚀 Configuración y Compilación

### Requisitos
- **CMake** 3.10 o superior
- **Compilador C++20**: 
  - Linux: `gcc` / `g++`
  - Windows: Visual Studio 2019+ o MinGW
  - macOS: Clang

### Instalación rápida

#### Linux (Ubuntu/Debian)
```bash
sudo apt-get install cmake g++ build-essential
```

#### Windows
- Descarga Visual Studio Community: https://visualstudio.microsoft.com/
- O instala MinGW: https://www.mingw-w64.org/

#### macOS
```bash
brew install cmake
```

---

## 📋 Compilar el proyecto

### 1. Clonar el repositorio
```bash
git clone https://github.com/SaulMoralesZ/cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita.git
cd cs2013-2026-2-proyecto-1-grupo3-EquipoDinamita
```

### 2. Crear carpeta de compilación
```bash
mkdir build
cd build
```

### 3. Generar archivos de compilación

**Linux/macOS:**
```bash
cmake ..
```

**Windows (Visual Studio):**
```bash
cmake .. -G "Visual Studio 17 2022"
```

**Windows (MinGW):**
```bash
cmake .. -G "MinGW Makefiles"
```

### 4. Compilar
```bash
make
```

O en Windows con Visual Studio:
```bash
cmake --build .
```

### 5. Ejecutar
```bash
./proyecto
```

---

## 📁 Estructura del proyecto

```
proyecto/
├── CMakeLists.txt        # Configuración de compilación
├── src/                  # Código fuente (.cpp)
├── include/              # Headers (.h, .hpp)
├── docs/                 # Documentación
└── build/                # Carpeta de compilación (ignorada)
```

---

## 📚 Librerías

- **FTXUI v7.0.3**: Interfaz de terminal interactiva (se descarga automáticamente)
- **C++20**: Estándar de lenguaje usado en el proyecto

---

## 📊 Documentación con Excalidraw

### 🎨 ¿Qué es Excalidraw?

Excalidraw es una herramienta para crear diagramas y documentar visualmente nuestro código. Cada miembro del equipo puede:
- 🎯 Dibujar diagramas de arquitectura, flujos, componentes, etc.
- 🎨 Usar su propio estilo y creatividad
- 📝 Explicar sus diagramas con total libertad

### 📍 Acceder al Excalidraw compartido

**Link de la sesión compartida:**
```
https://excalidraw.com/#json=Eu9n-74ERvm1Kk6g4Lux8,flYbTyMF9DIFenXuZSylww
```

**Cómo usarlo:**
1. Abre el link arriba
2. Dibuja, documenta y explica tu parte del código
3. Cuando termines, **descarga el archivo** como `.excalidraw`
4. Renómbralo a `documentacion_grafico.excalidraw`
5. Sube el archivo al repositorio en la carpeta `docs/`

### 💾 Guardar los diagramas localmente

Es importante guardar los diagramas en el repo para tener un histórico y que todos puedan acceder:

```bash
# Después de descargar el archivo de Excalidraw
mv archivo_descargado.excalidraw documentacion_grafico.excalidraw
git add docs/documentacion_grafico.excalidraw
git commit -m "docs: Update graphic documentation diagrams"
git push origin main
```

---

## 🐛 Solución de problemas

### CMake no encontrado
```bash
cmake --version
```
Si no aparece, instala CMake según tu SO.

### Errores de compilación
- Asegúrate de tener C++20 o superior
- Linux: `g++ --version` debe ser >= 9.0
- Windows: Usa Visual Studio 2019 o superior

### FTXUI no descarga
```bash
# Limpia la carpeta build e intenta de nuevo
rm -rf build
mkdir build && cd build
cmake ..
```

---

## 👥 Contribuyentes
- Equipo Dinamita

---

Para más ayuda, contacta al equipo. 💪
