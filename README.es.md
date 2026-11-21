# Memoria by DSK

**Generador de atmósferas infinitas: un paisaje sonoro que nunca se repite.**

Instrumento gratuito VST3 / AU / Standalone de [DSK Music](https://www.dskmusic.com/memoria/) para ambient, cine y meditación.
**[Descarga](#descarga)** · [Página del producto](https://www.dskmusic.com/memoria/) · [Última versión](https://github.com/dskmusic/memoria-by-dsk/releases/latest)
Pulsa **Play** y seis capas (un drone que muta, un pad, una textura granular, shimmer, viento y campanas) entran y salen solas,
mientras la armonía, la luz y el espacio evolucionan despacio durante minutos u horas sin repetirse nunca.

![Paisaje de Memoria](docs/screenshots/landscape.png)

## Funciones

- **Motor de paisaje vivo:** siete capas que tocan solas: Drone, Pad, Textura, Shimmer, Viento, Campanas y Bajo. Cada una tiene Nivel,
  Presencia (cuánto tiempo está presente), Tono y Carácter; una modulación lenta que nunca se repite decide cuándo aparece.
- **Sonido rico:** drone Spectral Mutation, pad analógico PWM + sierra con chorus de ensemble y apertura del filtro de los 80, nubes
  granulares afinadas a octavas y quintas, shimmer, bajo que sigue el acorde, frases de campanas y una nueva reverb tipo hall con pre-delay.
- **Armonía en movimiento:** 12 escalas, cualquier tonalidad, de quintas abiertas a acordes de 7ª y 9ª, progresiones reales (I-IV-V-vi, i-VI-III-VII de cine) con conducción de voces suave
  o **sincronizados al tempo de tu DAW** (cada 1-16 compases).
- **Cuatro macros grandes:** Evolución, Densidad, Luz (de noche a mediodía) y Espacio (de una sala a una catedral infinita), y **Congelar** sin chasquidos.
- **Play / Stop** que también sigue a tu DAW. La evolución sale de una **semilla** y de la posición en la línea de tiempo: lo que exportas
  suena igual que lo que oíste, y la misma semilla siempre da el mismo viaje.
- **Seis paisajes animados** que reaccionan a cada capa: Valle, Campo estelar, Bajo el agua, Neón, Océano y Aurora, con **pantalla completa**,
  atajos de teclado y **modo relax** sin textos.
- **14 estados de ánimo, 292 presets de fábrica:** Cine, Ambient, Meditación, Oscuro, Sci-Fi, Etéreo, Océano, Nostalgia, Épico, Bosque,
  Desierto, Dormir y los melódicos **Cuento** y **Banda sonora**, con una capa de **Melodía** compuesta (temas de caja de música, celesta
  o flauta suave que se repiten, se responden y resuelven sobre los acordes). El generador prueba varios candidatos, los comprueba y se queda con el más vivo.
- **Exportación de audio largo:** de 1 minuto a 3 horas en WAV, FLAC u OGG. **Vídeo del paisaje:** un MP4 con el paisaje animado y su sonido,
  listo para YouTube (usa ffmpeg, gratuito; Memoria lo encuentra o te ayuda a instalarlo).
- **Modo Dormir** y temporizador, afinación a **432 Hz**, **generador de ruido** (rosa, marrón, blanco, olas, vinilo).
- **8 memorias rápidas** con fundido de 2 segundos, navegador de presets con etiquetas y favoritos, A/B, deshacer/rehacer, MIDI learn,
  y cambios de preset sin chasquidos.
- Ventana escalable y maximizable, interfaz en **inglés y español** (automática o manual) con una página de **Ayuda** clara y amena.

| | |
|---|---|
| ![Neón](docs/screenshots/landscape-neon.png) | ![Bajo el agua](docs/screenshots/landscape-underwater.png) |
| ![Océano](docs/screenshots/landscape-ocean.png) | ![Aurora](docs/screenshots/landscape-aurora.png) |

## Descarga

**Página del producto:** https://www.dskmusic.com/memoria/

| Sistema | Recomendado | Otros formatos |
|---|---|---|
| **Windows** 10/11 (64 bits) | [Instalador (.exe)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-win-x64-setup.exe) | [VST3 (zip)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-win-x64-vst3.zip) · [Standalone (zip)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-win-x64-standalone.zip) |
| **macOS** 11+ (Universal: Apple Silicon + Intel) | [Instalador (.pkg)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-macos-universal-installer.pkg) | [VST3](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-macos-universal-vst3.zip) · [AU](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-macos-universal-au.zip) · [Standalone](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-macos-universal-standalone.zip) |
| **Linux** x86_64 | [VST3 (tar.gz)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-linux-x64-vst3.tar.gz) | [Standalone (tar.gz)](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/Memoria-by-DSK-linux-x64-standalone.tar.gz) |

Todos los archivos y notas: [última versión](https://github.com/dskmusic/memoria-by-dsk/releases/latest) · checksums: [SHA256SUMS.txt](https://github.com/dskmusic/memoria-by-dsk/releases/latest/download/SHA256SUMS.txt)

> Los binarios no están firmados. **Windows:** si aparece SmartScreen, pulsa *Más información* → *Ejecutar de todas formas*.
> **macOS:** si dice que no se puede abrir o que está dañado, haz clic derecho → *Abrir*, o ejecuta `xattr -dr com.apple.quarantine <archivo>` en Terminal.

### Primeros pasos

1. Carga **Memoria** como instrumento y pulsa **Play** (en el plugin o en el transporte de tu DAW).
2. Gira **Evolución**, **Densidad**, **Luz** y **Espacio**, elige un paisaje o pulsa **GENERAR** en la página Generador.
3. Render largo para vídeos o meditación: página **Exportar** → duración → WAV, FLAC u OGG.

Los presets y ajustes se guardan en `Documentos/DSK Music/Memoria/`.

## Compilar

Ver la sección *Build from source* del [README en inglés](README.md).

## Licencia

GNU GPL v3 (ver [LICENSE](LICENSE)). Hecho con JUCE (GPLv3) y el VST3 SDK (MIT). Fuente Orbitron con licencia SIL OFL.
Partes del motor (Spectral Mutation, reverb FDN, shimmer) vienen de [Genesix by DSK](https://github.com/dskmusic/genesix-by-dsk).
