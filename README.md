# LatinoVox 2D 🎮

> **Un motor gráfico y sandbox de vóxeles impulsado por C, Raylib y el Lenguaje Latino.**

[![Licencia: MIT](https://img.shields.io/badge/Licencia-MIT-blue.svg)](LICENSE)
[![Lenguaje](https://img.shields.io/badge/Lenguaje-Latino%20%2B%20C-orange.svg)]()
[![Motor Gráfico](https://img.shields.io/badge/Gráficos-Raylib-red.svg)]()
[![Estado](https://img.shields.io/badge/Versión-v0.2.0--alpha-green.svg)]()

---

## 🚀 Propósito y Objetivo

**LatinoVox 2D** nace como un proyecto de exploración técnica y diversión pura: construir paso a paso un entorno *sandbox* de bloques bidimensional estilo **Terraria** o **Luanti / Minetest**, pero utilizando al **Lenguaje Latino** (un lenguaje de programación de código abierto con sintaxis en español) como el motor de lógica y *Frontend*.

Más allá de crear un juego, el objetivo principal es expandir las fronteras y capacidades de Latino, demostrando que mediante una arquitectura híbrida eficiente es perfectamente apto para el desarrollo de **Frontend, interfaces gráficas de usuario (GUI/UI) y videojuegos interactivos en español**.

---

## 🏛️ Arquitectura del Motor Híbrido

Para garantizar un rendimiento óptimo sin sacrificar la flexibilidad del código en español, el motor opera bajo el principio rector:
> **"Hot Path en C, Cold Path en Latino"**

1. **La Capa de C y Raylib (Hot Path):** Maneja el procesamiento gráfico pesado a **60 FPS constantes**, el bucle principal, la detección de entrada por teclado y ratón, el recorte espacial (*View Culling*) y la renderización en tiempo real.
2. **La Capa de Latino (Cold Path):** Dicta las reglas del juego de alto nivel, la ontología de los bloques, la interfaz gráfica (GUI) y la generación procedural del mundo.
3. **El Puente FFI (Foreign Function Interface):** Conecta ambos mundos permitiendo que los scripts `.lat` modifiquen el estado del juego de forma segura sin recompilar el núcleo anfitrión.

---

## ✨ Características Principales (v0.2.0)

* **Motor Gráfico de Alto Rendimiento:** Escrito en C y Raylib, optimizado con *View Culling* para mantener 60 FPS estables incluso en pantallas de alta resolución.
* **Intérprete Incrustado (*Interpreter Embedding*):** La Máquina Virtual de Latino vive dentro del ejecutable en C.
* **Recarga en Caliente (*Hot-Reloading*):** Modifica los scripts en español, guarda y presiona **`[R]`** para ver los cambios en el mundo o la interfaz al instante sin reiniciar el motor.
* **Registro Dinámico de Nodos (`motor.registrar_nodo`):** El código en C no tiene materiales fijos; Latino define los bloques (Pasto, Piedra, Agua, Cartel, Árbol, Camino, Arena, Lava), sus colores RGB y sus propiedades de colisión.
* **Mundo por Regiones (*Chunks* de 16×16):** Generación de terreno bajo demanda a través de ruido determinista (`motor.ruido`), creando un mapa explorable masivo de hasta 256×256 casillas.
* **Frontend GUI Basada en Eventos:** Paneles laterales interactivos declarados desde Latino con soporte de *hover* para el ratón, botones clicables y tipografía adaptable.

---

## 🛠️ Requisitos e Instalación (Linux / Debian 13 / Ubuntu)

### 1. Instalar dependencias del sistema
Asegúrate de tener instaladas las herramientas de compilación y las librerías gráficas y de audio necesarias:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake git \
    libgl1-mesa-dev libasound2-dev libx11-dev \
    libxcursor-dev libxinerama-dev libxrandr-dev \
    libxi-dev libxext-dev libxfixes-dev libxrender-dev
```

### 2. Compilar e instalar Raylib (si no está empaquetado)
```bash
git clone --depth 1 https://github.com/raysan5/raylib.git
cmake -S raylib -B raylib/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON
cmake --build raylib/build --parallel
sudo cmake --install raylib/build
sudo ldconfig
```

### 3. Clonar y compilar LatinoVox
```bash
git clone https://github.com/tu-usuario/LatinoVox.git
cd LatinoVox
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
```

---

## 🏃‍♂️ Ejecución

Una vez compilado el proyecto, puedes ejecutar el motor directamente desde la raíz del repositorio:

```bash
./build/motor
```

### Controles Principales:
* **`WASD` / `Flechas`:** Caminar de forma continua (modo crucero de 1 toque).
* **`ESPACIO`:** Detener la marcha.
* **`Shift` (Mantener):** Correr al doble de velocidad.
* **`Clic Izquierdo`:** Quitar bloque / talar.
* **`Clic Derecho`:** Construir el bloque seleccionado.
* **`1` al `7`:** Seleccionar herramientas y materiales de construcción.
* **`[TAB]`:** Ocultar o mostrar el panel GUI de Latino.
* **`[R]`:** Recargar el script de configuración en caliente (*Hot-Reloading*).
* **`[G]` / `[C]`:** Guardar y Cargar el estado del mundo en disco (`mundo_chunks.dat`).
* **`[B]`:** Mostrar u ocultar la rejilla de los *chunks* (16x16).
* **`F11`:** Alternar pantalla completa.

---

## 📂 Estructura del Proyecto

```text
LatinoVox/
├── CMakeLists.txt         # Configuración del sistema de compilación
├── .gitignore             # Archivos excluidos del control de versiones
├── scripts/
│   └── arranque.lat       # Script en español (Nodos, Chunks y GUI)
└── src/
    └── main.c             # Núcleo en C, Raylib, FFI y View Culling
```

---

## 📜 Licencia

Este proyecto se distribuye bajo los términos de la **Licencia MIT**. Consulta el archivo [LICENSE](LICENSE) para más detalles.

---
*Desarrollado con pasión por la comunidad hispanohablante de programación.* 🚀