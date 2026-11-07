# ☀️ Sunshine Renderer — İlerleme ve Geliştirme Günlüğü (Progress Log)

Bu belge, Sunshine Renderer motorunun geliştirilme sürecindeki tüm fazları, mimari kararları, teknik çıktıları ve sonraki adımları kayıt altında tutar. **Her yeni faz tamamlandığında bu dosya düzenli olarak güncellenir.**

---

## 📌 Proje Vizyonu & Felsefesi
> **"OpenGL Kolaylığı, Vulkan Performansı"**  
> Kullanıcıyı binlerce satırlık Vulkan boilerplate koduna, kriptik `VkResult` hatalarına ve manuel bellek bariyerlerine boğmadan; modern C++20, RAII ve temiz bir RHI (Render Hardware Interface) mimarisiyle yüksek performanslı grafik gücü sunmak.

---

## 🗺️ Faz İlerleme Çizelgesi

| Faz | Başlık | Durum | Çıktı / Örnek | Tarih |
|:---:|:---|:---:|:---|:---:|
| **Faz 0** | Vulkan Başlatma & GLFW Entegrasyonu | ✅ Tamamlandı | `build/Debug/00_clear_screen.exe` | 2026-10-06 |
| **Faz 1** | Sunshine API v0 (Frame Döngüsü & Clear) | ✅ Tamamlandı | Temiz Frame & Swapchain sarmalama | 2026-10-06 |
| **Faz 2** | RHI Soyutlama Katmanı & İlk Üçgen | ✅ Tamamlandı | `build/Debug/01_triangle.exe` | 2026-10-07 |
| **Faz 3** | 3D Kamera, Depth Buffer, Doku & 3D Model | ✅ Tamamlandı | `build/Debug/02_textured_model.exe` | 2026-10-07 |
| **Faz 4** | Kullanılabilirlik & Canlı Shader Hot-Reloading | ✅ Tamamlandı | `build/Debug/03_hot_reload.exe` | 2026-10-14 |
| **Faz 5** | Performans Doğrulama, PBR & Instancing | ✅ Tamamlandı | `build/Debug/04_benchmark.exe` | 2026-10-18 |
| **Faz 6** | Sahne Yöneticisi & 3D glTF 2.0 Model Yükleme | ✅ Tamamlandı | `build/Debug/05_gltf_model.exe` | 2026-10-18 |
| **Faz 7** | İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping) | ✅ Tamamlandı | `build/Release/06_shadows.exe` | 2026-11-02 |
| **Faz 8** | Render Graph & İleri Post-Processing (HDR Bloom) | ✅ Tamamlandı | `build/Release/07_render_graph_bloom.exe` | 2026-11-02 |
| **Faz 9** | Compute Shader & Multi-threaded Command Recording | ✅ Tamamlandı | `build/Release/08_gpu_particles.exe` | 2026-11-07 |

---

## 📝 Tamamlanan Fazların Detaylı Kayıtları

### 🔹 Faz 0: Vulkan Başlatma & GLFW Entegrasyonu
- **Mimari:** GLFW ile cross-platform pencere oluşturma, `volk` kütüphanesi ile çalışma zamanı Vulkan fonksiyon yükleme.
- **Backend:** `VulkanContext` ile Instance, Physical Device, Queue Family ve Logical Device oluşturuldu.
- **Entegre GPU:** Intel(R) UHD Graphics donanımı üzerinde Vulkan 1.4 API başarıyla başlatıldı.

### 🔹 Faz 1: Sunshine API v0 (Frame Yaşam Döngüsü)
- **Kapsam:** Ham Vulkan semaforları ve fence'leri kullanıcıdan gizlendi.
- **Sarmalama:** `Sunshine::Device`, `Sunshine::SwapchainContext`, `Sunshine::FrameContext`.
- **Erişim:** Kullanıcı `device.BeginFrame()` ve `device.SubmitFrame()` çağrılarıyla double/triple buffering işlemlerini zahmetsizce yönetebilir hale geldi.

### 🔹 Faz 2: RHI Soyutlama Katmanı & İlk Üçgen
- **RHI Arayüzleri:** `IRHIBuffer`, `IRHIShader`, `IRHIPipeline`, `IRHICommandList`.
- **Backend Gerçeklemesi:** `VulkanRHIBuffer`, `VulkanRHIShader`, `VulkanRHIPipeline`, `VulkanRHICommandList`.
- **Temel Geometri:** 2D renkli üçgen çizimi (`01_triangle`). Kullanıcı doğrudan Vulkan struct'ları yerine `Sunshine::VertexBuffer`, `Sunshine::Pipeline` sınıflarını kullandı.

### 🔹 Faz 3: 3D Kamera, Depth Buffer, Doku & 3D Model
- **3D Kamera Sistemi:** `Sunshine::Camera` sınıfı geliştirildi. Vulkan'ın Y ekseni tersliği (`proj[1][1] *= -1.0f`) donanıma tam uyumlu hale getirildi.
- **Derinlik Tamponu (Depth Buffer):** Intel UHD için optimal `VK_FORMAT_D32_SFLOAT` seçimi yapıldı, render pass renk + derinlik eklentileriyle güncellendi.
- **Doku & Sampler:** `stb_image` entegrasyonu, CPU -> GPU staging buffer transfer pipeline'ı, `Sunshine::Texture` ve `Sunshine::Sampler` sarmalayıcıları.
- **Uniform Buffer & Descriptors:** Çift tamponlu (`MAX_FRAMES_IN_FLIGHT = 2`) descriptor set ve uniform buffer yönetimi otomatikleştirildi.
- **Çıktı:** Sürekli dönen ve aydınlatılan damalı dokulu 3D küp (`02_textured_model`).

### 🔹 Faz 4: Kullanılabilirlik & Canlı Shader Hot-Reloading
- **Bağlamsal Hata Mesajları:** Kriptik `VkResult` sayıları yerine dosya, satır ve **eyleme geçirilebilir çözüm önerisi (`m_suggestion`)** sunan `GraphicsException` ve `VkResultToDiagnosis`.
- **Canlı Shader Hot-Reloading:** `Sunshine::ShaderWatcher` arka planda disk değişikliklerini takip edip `glslang.exe` derleyicisini çalıştırarak GPU pipeline'ını (`Pipeline::ReplaceRHIPipeline`) çalışma anında pencereyi kapatmadan değiştirmeyi başardı.
- **Sıfır Çökme Garantisi (Zero-Crash Tolerance):** Shader'da yazım hatası yapıldığında motor çökmeyip önceki çalışan pipeline'ı render etmeye devam eder, hata konsola basılır, düzeltilince anında güncellenir.
- **Dokümantasyon:** Tüm public API başlıkları Doxygen formatında temiz açıklamalarla belgelendi.
- **Çıktı:** Canlı renk ve efekt düzenlenebilen 3D küp (`03_hot_reload`).

### 🔹 Faz 5: Performans Doğrulama, PBR & Instancing
- **Hardware Instancing:** Çoklu binding per-instance vertex layout desteği (`VertexBindingDesc`, `VertexInputRate::PerInstance`).
- **Tek Draw Call:** `cmd.DrawIndexed(indexCount, instanceCount)` ile 2.500 - 5.000 arası bağımsız hareketli 3D küre tek bir çizim çağrısında render edildi.
- **Cook-Torrance Microfacet PBR:** GGX normal dağılımı (NDF), Smith's Schlick-GGX geometri gölgelemesi ve Fresnel-Schlick yansıma hesaplaması.
- **4 Dinamik Renkli Işık:** Kehribar ana ışık, camgöbeği ve yakut orbit ışıkları, zümrüt dolgu ışığı.
- **Post-Processing:** ACES Filmic tonemapping, sinematik vignette ve sRGB gamma 2.2 düzeltmesi (klavyeden `Space` ile açılıp kapatılabilir).
- **Gerçek Zamanlı Profiler:** `Sunshine::Profiler` ile FPS, Frame Time, poligon ve draw call metrikleri pencere başlığında anlık telemetri olarak sunuldu.
- **Çıktı:** Yüksek kare hızlı PBR stres testi demosu (`04_benchmark`).

### 🔹 Faz 6: Sahne Yöneticisi & 3D glTF 2.0 Model Yükleme
- **glTF 2.0 Desteği:** `tinygltf` kütüphanesi motor yapısına entegre edildi, hem ASCII `.gltf` hem de ikili paketlenmiş `.glb` dosyaları doğrudan GPU'ya yüklenebilir hale geldi.
- **Sahne Hiyerarşisi (`SceneNode` & `Transform`):** Ebeveyn-çocuk ilişkili (Parent-Child) dönüşüm matrisleri ($T \times R \times S$) geliştirildi, glTF düğüm ağacı eksiksiz ayrıştırıldı.
- **PBR Materyalleri & Dokular:** Base Color (Albedo), Normal, Metallic-Roughness (G/B kanalları) ve Occlusion (AO) dokuları otomatik olarak GPU `Texture2D` ve `Sampler` nesnelerine bağlandı. Eksik dokular için 1x1 piksellik fallback dokular üretildi.
- **Tangent-Space Normal Mapping:** Vertex shader'da $TBN$ matrisi hesaplandı; teğet uzayı normal haritaları fragment shader'da dünya uzayına dönüştürülerek mikro detaylı yüzey çıkıntıları sağlandı. Modelde teğet vektörleri eksik olduğunda otomatik MikkTSpace teğet hesaplama algoritması devreye alındı.
- **Etkileşimli Kamera & Dinamik Işıklar:** Fare ile orbit dönüşü, zoom kontrolleri ve dönen 4 dinamik renkli ışıkla endüstri standardı Khronos `DamagedHelmet.glb` modeli başarıyla render edildi.
- **Çıktı:** `build/Release/05_gltf_model.exe`

### 🔹 Faz 7: İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping)
- **Derinlik Odaklı Render Pass (`Sunshine::ShadowMap`):** Yüksek çözünürlüklü (2048x2048 `VK_FORMAT_D32_SFLOAT`) derinlik dokusu, özel offscreen render pass ve framebuffer mimarisi geliştirildi.
- **Vulkan Subpass Otomasyonu:** Render pass sonunda `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` geçişi ve subpass bağımlılığı (dependency) ile manuel bellek bariyerine gerek kalmadan derinlik tamponunun ana geçişte örneklenmesi garanti altına alındı.
- **Temiz İki Geçişli (Two-Pass) API:** `cmd.BeginShadowPass(shadowMap)` ve `cmd.EndShadowPass()` metotları ile gölge ve sahne geçişleri motor içinde yalın ve sezgisel hale getirildi.
- **Uyumlu Pipeline Oluşturma:** `device.CreatePipeline(pipeDesc, shadowMap)` ile sıfır renk eklentili (`colorAttachmentCount = 0`) gölge pipeline'ları ve swapchain pipeline'ları tek çatı altında önbelleklendi.
- **Donanım Derinlik Kaydırması (Depth Bias):** Rasterizer üzerinde statik constant ve slope depth bias etkinleştirilerek yüzey kendi kendine gölgelenmesindeki yapay bozulmalar (shadow acne / moire) yok edildi.
- **Yumuşak Gölgeler (PCF 3x3 Filtering):** `pbr_shadow.frag` shader'ında 9 noktalı Percentage Closer Filtering ve eğime duyarlı (slope-scaled) derinlik toleransı ile sinematik yumuşak gölge kenarları (penumbra) elde edildi.
- **Border Clamping:** Işık hacmi dışındaki koordinatların kararmaması için `VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE` kenarlık rengine sahip gölge sampler'ı eklendi.
- **Çıktı:** `build/Release/06_shadows.exe` (Dönen güneş ışığı altında zemine ve glTF kask modeline dinamik yumuşak gölgeler düşüren orbit kameralı etkileşimli sahne).

### 🔹 Faz 8: Render Graph & İleri Post-Processing (HDR Bloom)
- **HDR Format Desteği (`RGBA16_FLOAT`):** `TextureFormat::RGBA16_FLOAT` ve `VK_FORMAT_R16G16B16A16_SFLOAT` desteği eklendi. Renk değerlerinin $[0.0, 1.0]$ sınırına takılmadan yüksek dinamik aralıklı (HDR, örneğin $10.0+$ parlaklık değerleri) depolanması sağlandı.
- **Offscreen Render Target Mimarisi (`Sunshine::RenderTarget`):** 16-bit float renk eki ve 32-bit float derinlik ekine sahip, özel render pass ve framebuffer yöneten, sahne geçişi sonrasında diğer pass'lerde doğrudan `Texture` gibi örneklenebilen (`GetColorTexture()`, `GetColorImageView()`) bağımsız render hedefi sınıfı geliştirildi.
- **DAG Tabanlı `Sunshine::RenderGraph`:**
  - **Bağımlılık Analizi & Sıralama:** Kahn's Algoritması ile yönlendirilmiş çevrimsiz çizge (DAG) üzerinde geçişlerin (`RenderPassBuilder::Read` ve `Write`) girdi/çıktı ilişkilerine göre otomatik topolojik sıralanması sağlandı.
  - **Otomatik Düzen (Layout) Geçişleri:** Vulkan render pass subpass bağımlılıkları (`EXTERNAL -> 0` ve `0 -> EXTERNAL`) ile render target'lar çizim bittiğinde otomatik `SHADER_READ_ONLY_OPTIMAL` düzenine geçirildi; manuel bellek bariyeri veya wait semaforu gereksinimi ortadan kaldırıldı.
  - **Fiziksel RenderTarget Havuzlama (Pooling & Reuse):** Geçişler compile edildiğinde aynı format ve boyuttaki geçici dokular çakışmayan yaşam döngülerine göre havuzdan eşleştirilerek GPU bellek tüketimi minimuma indirildi.
- **Çok Geçişli (Multi-Pass) Dual-Kawase / Pyramid Bloom Pipeline:**
  1. `SceneHDR`: 3D glTF PBR kask modeli, yansımalı damalı zemin ve 4 adet yörüngede dönen ultra-parlak neon küre tam HDR 16-bit float arabelleğine render edilir.
  2. `BrightFilter`: Luma eşik filtresi ($L > 1.0$) ile sahnenin yalnızca yüksek enerjili parlayan bölgeleri 1/2 çözünürlükteki hedef dokuya süzülür.
  3. `BloomDown1` & `BloomDown2`: 1/4 ve 1/8 çözünürlükte aşamalı Dual-Kawase / tent downsample piramidi ile ışık geniş alana homojen dağıtılır.
  4. `BloomUp1` & `BloomUp2`: Aşamalı upsampling ve additive blend ile piramit katmanları birleştirilerek yumuşak, sinematik parlama elde edilir.
  5. `Composite`: Orijinal HDR sahne ile Bloom piramidi ağırlıklandırılarak birleştirilir, ACES Filmic tonemapping ve sRGB Gamma 2.2 düzeltmesi uygulanarak doğrudan Swapchain penceresine aktarılır.
- **Prosedürel Fullscreen Quad:** Vertex veya index arabelleğine ihtiyaç duymayan, `gl_VertexIndex` ile GPU üzerinde dinamik 3 köşeli devasa üçgen üreten sıfır-overhead post-process vertex shader mimarisi (`cmd.Draw(3)`).
- **Etkileşimli Kontroller & Canlı Telemetri:**
  - Sol Fare Sürükle: Orbit kamera dönüşü, Tekerlek: Zoom.
  - `B` Tuşu: Bloom efektini anında açıp kapatma (A/B karşılaştırması).
  - `+` / `-` Tuşları: Bloom yoğunluğunu canlı artırma / azaltma.
  - `Space`: Işık yörüngelerini duraklatma / devam ettirme.
  - `R`: Kamerayı başlangıç konumuna sıfırlama.
  - Pencere başlığında anlık FPS, ms ve Bloom durumu telemetrisi.
- **Çıktı:** `build/Release/07_render_graph_bloom.exe`

### 🔹 Faz 9: Compute Shader & Multi-threaded Command Recording
- **Compute Pipeline Mimarisi:**
  - RHI katmanına `IRHIComputePipeline` ve `ComputePipelineDesc` arayüzleri eklendi.
  - Vulkan backend'inde `VulkanRHIComputePipeline` gerçeklendi; render pass ve framebuffer bağımlılığı olmadan doğrudan `vkCreateComputePipelines` ile sıfır-overhead hesaplama hattı kuruldu.
  - Sunshine frontend katmanına `Sunshine::ComputePipeline`, `device.CreateComputePipeline(...)`, `cmd.SetComputePipeline(...)` ve `cmd.Dispatch(x, y, z)` API'leri entegre edildi.
- **Storage Buffer (SSBO) Desteği:**
  - `BufferUsage::Storage` ve `DescriptorType::StorageBuffer` mimariye dahil edildi.
  - `Sunshine::StorageBuffer<T>` sınıfı geliştirildi. `AsVertexBuffer()` metodu sayesinde Compute Shader tarafından yazılan fizik simülasyonu GPU tamponu, CPU'ya hiçbir veri kopyalanmadan doğrudan Graphics Pipeline'ında `VertexBuffer` olarak bağlandı.
- **Compute-to-Graphics Senkronizasyon Bariyeri:**
  - `cmd.ComputeToGraphicsBarrier()` çağrısı ile Vulkan bellek bariyeri (`VkMemoryBarrier`) otomatikleştirildi; `VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT` yazımları güvenle `VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT` okumalarına senkronize edildi.
- **Çoklu İş Parçacıklı Komut Kaydı (Multi-threaded Command Recording):**
  - **İş Parçacığı Havuzu (`Sunshine::WorkerThreadPool`):** Donanım çekirdek sayısına (`hardware_concurrency`) göre dinamik iş parçacığı yöneten, `ParallelFor` ve `Enqueue` yetenekli modern C++20 thread pool yazıldı.
  - **Thread-Safe Komut Havuzları:** Vulkan spesifikasyonunun gerektirdiği gibi her iş parçacığı ve frame slotu için bağımsız `VkCommandPool` tahsisi (`DeviceImpl::GetThreadCommandPool`) sağlandı.
  - **İkincil Komut Tamponları (`Sunshine::SecondaryCommandContext`):** `VK_COMMAND_BUFFER_LEVEL_SECONDARY` seviyesinde tamponlar tahsis edilerek, render geçişi `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS` ile başlatıldı.
  - **Paralel Enjeksiyon (`cmd.RecordParallel`):** Sahnedeki çizim iş yükü $N$ iş parçacığına bölüştürüldü; her çekirdek kendi ikincil tamponunu eşzamanlı doldurdu ve ana iş parçacığı `vkCmdExecuteCommands` ile tek bir Vulkan çağrısında tüm çizimleri GPU'ya iletti.
- **131.072 GPU Parçacık Simülasyonu (`08_gpu_particles`):**
  - %100 GPU üzerinde Compute Shader (`particle.comp`) ile 131.072 parçacığın dinamik fiziği simüle edildi.
  - **3 Simülasyon Modu:**
    1. *Attractor Dance:* 4 adet dinamik hareketli çekim merkezi ile yerçekimi ve girdap dansı.
    2. *Cosmic Vortex:* Y ekseni etrafında spiral girdap (tornado) fiziği ve yukarı yönlü emilim.
    3. *Galactic Supernova:* Nabız gibi atan kozmik patlama, şok dalgaları ve küresel sınır yansımaları.
  - Parçacık hızına bağlı dinamik renk gradyanı (Mavi -> Macenta -> Altın Sarısı -> Saf Beyaz).
  - Additive blending (`BlendDesc::blendEnable = true`) ve radyal Gaussian falloff ile parlayan kozmik nebula efekti.
  - `T` tuşu ile Canlı Kıyaslama: Tek İş Parçacıklı vs Çoklu İş Parçacıklı (Secondary Command Buffers) komut kaydı performansı.
  - `Space` ile simülasyonu duraklatma, `1`-`2`-`3` ile mod geçişi, `R` ile kamera sıfırlama, orbit/zoom kamera kontrolleri ve canlı FPS/ms telemetrisi.
- **Çıktı:** `build/Release/08_gpu_particles.exe`

---

## 🔮 Gelecek Fazlar (Roadmap 2)
Detaylı gelecek hedefleri ve yeni mimari adımlar için [ROADMAP2.md](file:///c:/Users/ibrah/OneDrive/Desktop/Sunshine%20renderer/ROADMAP2.md) belgesine başvurunuz.
