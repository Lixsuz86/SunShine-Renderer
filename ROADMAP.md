# Sunshine Renderer — Roadmap

> **Vizyon:** OpenGL kadar kolay anlaşılır, Vulkan kadar performanslı bir renderer.
> Vulkan veya OpenGL'in API'lerini birebir kopyalamıyoruz — kendi tasarımımızı,
> kendi isimlendirmemizi, kendi mental modelimizi kuruyoruz. Amaç, düşük seviye
> GPU programlamanın performans avantajlarını korurken, kullanıcı (yani gelecekte
> bu renderer'ı kullanan sen) tarafında bilişsel yükü (cognitive load) minimuma
> indirmek.

---

## 0. Mimari Karar: "Custom Backend" Ne Anlama Geliyor?

Gerçek bir GPU sürücüsü yazmak (driver seviyesinde) pratik değil ve bu projenin
kapsamının çok dışında. Bunun yerine:

- **Alt katman (backend):** Vulkan kullanılacak. Sebep: Vulkan, GPU'ya en yakın,
  en az soyutlamaya sahip, çapraz platform (cross-platform) API. Performans
  tavanı Vulkan'ın tavanı ile aynı olacak.
- **Üst katman (frontend / public API):** Sunshine'ın kendi API'si. Bu katman
  OpenGL'in "hemen çiz" (immediate, az boilerplate) hissini taklit eder ama
  altta Vulkan'ın command buffer / pipeline / descriptor mantığını kullanır.
  Bu karmaşıklık kullanıcıdan **gizlenir**, ortadan kaldırılmaz.
- İleride (v2+) isteğe bağlı olarak DirectX 12 veya Metal gibi ek backend'ler
  eklenebilir — bu yüzden frontend katmanı, backend'e sıkı sıkıya bağlı
  (tightly coupled) olmayacak şekilde tasarlanacak (bkz. Faz 2: RHI).

Bu, "custom backend" isteğinin gerçekçi ve inşa edilebilir hâli. Roadmap
boyunca "backend" dediğimizde Vulkan'ı, "frontend" veya "Sunshine API" dediğimizde
senin yazacağın/kullanacağın kolay API'yi kastediyoruz.

---

## Genel İlkeler

1. **Sıfırdan, framework'süz.** Vulkan dışında (ki o da bir "driver arayüzü",
   framework değil) hazır bir render motoru/kütüphanesi (bgfx, Diligent, vs.)
   kullanılmayacak. Pencere/yükleyici gibi yardımcı kütüphaneler (GLFW, stb_image,
   volk gibi ince yükleyiciler) kabul edilebilir — bunlar renderer'ın kendisi değil.
2. **Portable, sürücü seviyesine inmeden.** Windows + Linux hedeflenir (macOS
   MoltenVK ile mümkün ama öncelikli değil). Vulkan zaten bunu sağlıyor.
3. **Her fazın sonunda çalışan bir demo olacak.** "Bir şey görünmeden" bir
   sonraki faza geçilmez.
4. **Orta seviye pipeline ilk hedef.** Ne "sadece üçgen çiz" kadar minimal, ne
   PBR/ileri teknikler kadar ileri. Faz 1–3, klasik bir forward renderer'ın
   temel taşlarını (pencere, pipeline, buffer, texture, basit sahne, kamera)
   kapsar.
5. **Donanım gerçeği:** Geliştirme, Intel UHD entegre grafik üzerinde yapılacak.
   Bu yüzden her fazda "entegre GPU'da çalışıyor mu?" kontrolü olacak; ağır
   teknikler (ray tracing vs.) yol haritasının çok ilerisinde, opsiyonel bir
   faz olarak durur.

---

## Faz 0 — Ortam ve İskelet (Setup)

**Hedef:** Boş bir pencere açılıyor, Vulkan başlatılıyor, hiçbir hata yok.

- [x] Proje dizin yapısı: `engine/`, `backend/vulkan/`, `examples/`, `third_party/`
- [x] Build sistemi: CMake (portable olması için)
- [x] Pencere/girdi: GLFW entegrasyonu (sadece pencere + input, render değil)
- [x] Vulkan yükleyici: volk veya elle `vkGetInstanceProcAddr` zinciri
- [x] `VkInstance`, validation layers (debug modda), `VkPhysicalDevice` seçimi
  (entegre GPU'yu doğru tanıma ve loglama dahil)
- [x] `VkDevice`, queue family seçimi (graphics + present)
- [x] Swapchain oluşturma, temel resize/minimize yönetimi
- [x] "Clear color" demo: pencere açılıyor, ekran tek renkle temizleniyor

**Çıktı:** `examples/00_clear_screen`

---

## Faz 1 — Sunshine API'nin İlk Taslağı (Frontend v0)

**Hedef:** Vulkan'ın boilerplate'ini gizleyen ilk API katmanı.

- [x] `Device`, `SwapchainContext`, `CommandContext` gibi Sunshine-native sınıflar
  tasarlanır (isimlendirme Vulkan/OpenGL'i birebir taklit etmez)
- [x] Basit bir "frame lifecycle" API'si: `BeginFrame()` / `SubmitFrame()` gibi —
  kullanıcı sync objelerini (semaphore/fence) elle yönetmez
- [x] Command buffer kaydı Sunshine tarafında sarmalanır (`frame.Clear()`, `CommandContext`)
- [x] Hata yönetimi stratejisi netleştirilir (`Sunshine::GraphicsException` +
  `std::optional<FrameContext>` — AI_RULES.md'de sabitlendi)

**Çıktı:** Faz 0'daki demo, artık ham Vulkan çağrıları yerine Sunshine API'si
ile yazılır.
- **Ham Vulkan Demo:** 372 satır
- **Sunshine API Demo:** 102 satır (saf render/setup mantığında ~340 satırdan ~35 satıra düşüş, **%90 azalma**)
- **Bilişsel Yük:** Sıfır çıplak Vulkan handle'ı, sıfır elle sync yönetimi.

---

## Faz 2 — Render Hardware Interface (RHI) İskeleti

**Hedef:** Gelecekte başka backend eklenebilecek şekilde soyutlama sınırlarını
çizmek (şimdilik tek implementasyon: Vulkan).

- [x] `IDevice`, `IBuffer`, `ITexture`, `IPipeline`, `ICommandList` gibi soyut
  arayüzler tanımlanır (`engine/include/sunshine/rhi/`)
- [x] Vulkan implementasyonu bu arayüzlerin arkasına taşınır (`backend/vulkan/rhi/`)
- [x] Shader derleme pipeline'ı: GLSL → SPIR-V derleme desteği ve taşınabilir bytecode entegrasyonu
- [x] Basit bir materyal/pipeline state cache sistemi (hash-tabanlı pipeline cache uygulandı ve doğrulandı)

**Çıktı:** `examples/01_triangle` — üçgen artık RHI arayüzü üzerinden çizilir.

---

## Faz 3 — Temel Render Pipeline

**Hedef:** Gerçek bir sahneyi çizebilen orta seviye bir renderer.

- [x] Vertex/Index buffer yönetimi (staging buffer + CPU/GPU memory transferleri)
- [x] Uniform buffer yönetimi, descriptor set yönetimi Sunshine tarafında otomatikleştirildi (kullanıcı elle descriptor pool yönetmez)
- [x] Texture yükleme (stb_image entegrasyonu, Texture2D, Sampler yönetimi)
- [x] Basit kamera sistemi (Vulkan clip space Y-inversion uyumlu view/projection matrisleri)
- [x] Depth buffer / depth testing (D32_SFLOAT swapchain & render pass entegrasyonu)
- [x] 3D mesh ve doku yönetimi
- [x] Faz sonunda: döndürülebilir ve derinlik testli dokulu bir 3D model sahnede

**Çıktı:** `examples/02_textured_model`

---

## Faz 4 — Kullanılabilirlik ve "OpenGL Hissi" Cilası

**Hedef:** API'yi gerçekten "kolay" yapan detaylar.

- [x] Hata mesajları: Vulkan'ın kriptik `VkResult` kodları yerine anlaşılır,
  bağlamsal hata mesajları (dosya/satır, hangi kaynak, ne yanlış gitti, çözüm önerileri)
- [x] Debug modunda bilgilendirici validation layer raporlama ve taşınabilir fallback
- [x] Canlı Shader Hot-Reloading (`ShaderWatcher` - disk takibi, runtime'da otomatik SPIR-V derleme ve GPU üzerinde dinamik pipeline swap)
- [x] Sağlam resource yaşam döngüsü (RAII tabanlı, sıfır sızıntı, elle `vkDestroy*` çağrılmaz)
- [x] Dokümantasyon: Tüm public API başlıkları ve sınıfları için kapsamlı Doxygen dokümantasyonu

**Çıktı:** `examples/03_hot_reload` (Canlı shader reload edilebilen dokulu 3D küp)

---

## Faz 5 — Performans Doğrulama ve İleri Seviyeler

**Hedef:** "Vulkan kadar performanslı" iddiasını ölçmek ve ileri teknikleri
değerlendirmek.

- [x] Basit ve güçlü bir benchmark sahnesi (`04_benchmark`): 2.500 - 5.000 arası nesne stres testi
- [x] Donanım destekli Instanced Rendering (`DrawIndexed(indexCount, instanceCount)` ve çoklu binding per-instance vertex layout)
- [x] Gerçek zamanlı telemetri ve Profiler sistemi (`Sunshine::Profiler`: FPS, Frame Time, Triangles, Draw Calls)
- [x] Cook-Torrance Microfacet PBR materyaller (GGX normal dağılımı, Smith geometri gölgeleme, Fresnel-Schlick)
- [x] 4 Dinamik renkli ışık kaynağı ve atenüasyon
- [x] Post-processing pipeline: ACES filmik tonemapping, sinematik vignette ve gamma 2.2 düzeltmesi (klavyeden Space tuşu ile anlık toggle)

**Çıktı:** `examples/04_benchmark` (2.500+ PBR küre, tek draw call, canlı telemetri ve ACES post-processing)

---

## Milestone Özeti

| Faz | Çıktı | Durum |
|---|---|---|
| 0 | Boş pencere + Vulkan init | ✅ Tamamlandı |
| 1 | Sunshine API v0 (frame lifecycle sarmalama) | ✅ Tamamlandı |
| 2 | RHI arayüzleri + üçgen demo | ✅ Tamamlandı |
| 3 | Texture'lı 3D model, kamera, depth | ✅ Tamamlandı |
| 4 | Geliştirici deneyimi cilası (hata mesajları, hot-reload) | ✅ Tamamlandı |
| 5 | Performans doğrulaması + PBR + Instancing + Post-Process | ✅ Tamamlandı |

---

## Açık Sorular (İlerledikçe netleştirilecek)

- Shader dili: sadece GLSL mi, yoksa Sunshine'a özgü basitleştirilmiş bir
  shading language mi düşünülecek (uzun vadeli, opsiyonel bir fikir)?
- Memory allocator: VMA (Vulkan Memory Allocator) kullanılsın mı, yoksa basit
  bir custom allocator mı yazılsın? (Framework kullanmama ilkesiyle gerilim
  var — VMA bir "yardımcı kütüphane" mi yoksa "renderer'ın kendisi" mi
  sayılır, netleştirilmeli.)
- Hedef minimum Vulkan sürümü (1.1 mi 1.3 mü) — entegre Intel UHD sürücü
  desteğine göre karar verilecek.
