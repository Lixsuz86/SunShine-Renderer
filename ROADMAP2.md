# Sunshine Renderer — Yol Haritası 2.0 (ROADMAP 2)

Bu belge, ilk 5 fazı başarıyla tamamlanan Sunshine Renderer motorunun **ikinci nesil (Next-Gen) mimari hedeflerini** tanımlar.

Sunshine Renderer'ın temel ilkesi değişmez:  
> **"OpenGL Kolaylığı, Vulkan Performansı"**  
> Kullanıcı kod yazarken modern, sezgisel ve temiz bir C++20 API'si görür; arka planda ise modern Vulkan'ın saf gücü ve sıfır sürücü ek yükü çalışır.

---

## 🧭 Milestone Özeti (Roadmap 2)

| Faz | Başlık | Hedef Çıktı | Durum |
|:---:|:---|:---|:---:|
| **Faz 6** | Sahne Yöneticisi & 3D Model Yükleme (glTF 2.0 / OBJ) | `examples/05_gltf_model` | ✅ Tamamlandı |
| **Faz 7** | İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping) | `examples/06_shadows` | ✅ Tamamlandı |
| **Faz 8** | Render Graph & İleri Post-Processing (Bloom / SSAO) | `examples/07_render_graph_bloom` | ✅ Tamamlandı |
| **Faz 9** | Compute Shader & Multi-threaded Command Recording | `examples/08_gpu_particles` | ✅ Tamamlandı |
| **Faz 10** | Sunshine Editör & Debug GUI (ImGui Entegrasyonu) | `examples/09_editor` | ⏹️ İptal Edildi (v2.0 Kapsamına Ertelendi) |

---

## 📌 Faz Detayları

### Faz 6 — Sahne Yöneticisi & 3D Model Yükleme (glTF 2.0 / OBJ)
**Hedef:** Kod içinde elle köşe dizileri tanımlamak yerine gerçek 3D sanat varlıklarını (asset) sahneye aktarmak.

- [x] Hafif, framework bağımsız model yükleyici entegrasyonu (`tinygltf`).
- [x] `Mesh` ve `Submesh` veri yapıları: Çoklu materyale sahip karmaşık 3D modellerin ayrıştırılması.
- [x] `SceneNode` ve Sahne Hiyerarşisi: Ebeveyn-çocuk ilişkili (Parent-Child) yerel/dünya dönüşüm matrisleri (`Transform`).
- [x] glTF 2.0 PBR Doku Desteği:
  - Base Color (Albedo) haritası
  - Normal haritası (Tangent space normal mapping)
  - Metallic-Roughness kompozit haritası
  - Emissive & Ambient Occlusion (AO) haritaları
- [x] Faz Sonunda: Sahnede tam dokulu, gerçekçi bir 3D glTF modeli (Khronos DamagedHelmet) serbest orbit kamera ile incelenebilecek.

**Çıktı:** `examples/05_gltf_model`

---

### Faz 7 — İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping)
**Hedef:** Sahneye derinlik ve gerçekçilik katan dinamik gölge sistemleri.

- [x] Sadece derinlik yazan (Depth-Only) Render Pass & Shadow Map Pipeline'ı (`Sunshine::ShadowMap`).
- [x] Yönlü Işık (Directional Light) için Light Space Matrix dönüşümü ve gölge projeksiyonu.
- [x] Donanım derinlik kaydırması (Depth Bias - Constant & Slope factor) ile shadow acne engelleme.
- [x] PCF (Percentage Closer Filtering) 3x3 örnekleme ile yumuşak kenarlı gölgeler (Soft Shadows).
- [x] Cook-Torrance PBR gölge fragment shader'ı (`pbr_shadow.frag` & `pbr_shadow.vert`).
- [x] Temiz iki geçişli API mimarisi: `cmd.BeginShadowPass(shadowMap)` ve `cmd.EndShadowPass()`.
- [x] Faz Sonunda: Dönen güneş ışığı altında zemine ve glTF kask modeline dinamik yumuşak gölgeler düşüren orbit kameralı demo.

**Çıktı:** `examples/06_shadows`

---

### Faz 8 — Render Graph & İleri Post-Processing (Bloom / SSAO)
**Hedef:** Çoklu render geçişlerini (pass) ve doku bağımlılıklarını otomatik yöneten modern mimari.

- [x] Basit ve Güçlü `RenderGraph` / `FrameGraph`:
  - Pass'ler arası görsel bağımlılıkların (dependency) otomatik çözümü (Kahn's Topological Sort).
  - Vulkan `VkImageMemoryBarrier` ve layout geçişlerinin (Color Attachment -> Shader Read) el değmeden otomasyonu.
  - Geçici render target dokularının bellek havuzunda yeniden kullanılması (Physical RenderTarget Pooling & Reuse).
- [x] Yüksek Kaliteli Bloom (Dual-Kawase Blur / Downsample & Upsample piramidi).
- [x] HDR Floating-Point Format (`RGBA16_FLOAT`, `VK_FORMAT_R16G16B16A16_SFLOAT`) & RenderTarget soyutlaması.
- [x] Prosedürel Fullscreen Quad (`gl_VertexIndex` bazlı 0-buffer pass pipeline).
- [x] ACES Filmic Tone Mapping & Gamma Düzeltmesi ile Swapchain Kompozitleme.
- [x] Faz Sonunda: 3D glTF model, yansımalı zemin ve dönen ultra-parlak neon küreler üzerinde sinematik HDR Bloom parlamaları.

**Çıktı:** `examples/07_render_graph_bloom`

---

### Faz 9 — Compute Shader & Multi-threaded Command Recording
**Hedef:** GPU genel hesaplama gücünü ve çok çekirdekli CPU paralelizmini motora entegre etmek.

- [x] Compute Pipeline Arayüzü: `IRHIComputePipeline`, `VulkanRHIComputePipeline`, `device.CreateComputePipeline`, `cmd.Dispatch(x, y, z)`.
- [x] Storage Buffer (SSBO) Desteği: `BufferUsage::Storage`, `StorageBuffer<T>`, aynı GPU arabelleğini hem Compute hem Vertex Buffer olarak kullanma (`AsVertexBuffer()`).
- [x] Compute-to-Graphics Senkronizasyonu: `cmd.ComputeToGraphicsBarrier()` ile Compute yazımlarını Vertex Input okumalarına bağlayan bellek bariyeri.
- [x] Çoklu İş Parçacıklı Komut Kaydı (Multi-threaded Command Recording):
  - İş parçacığı havuzu (`WorkerThreadPool`) ve `ParallelFor` iş dağıtımı.
  - Thread-safe per-thread `VkCommandPool` yönetimi.
  - İkincil komut tamponları (`SecondaryCommandContext` & `cmd.BeginSecondaryPass` & `cmd.RecordParallel`).
- [x] GPU Tabanlı Parçacık Simülasyonu: 131.072 parçacık (%100 GPU üzerinde çekim merkezleri, girdap/tornado ve süpernova fiziği, additive blending).
- [x] Faz Sonunda: 130.000+ parçacığın yüksek kare hızlarında etkileşimi ve tek iş parçacığı vs çoklu iş parçacığı kıyaslaması.

**Çıktı:** `examples/08_gpu_particles`

---

### Faz 10 — Sunshine Editör Arayüzü & Debug GUI (ImGui Entegrasyonu)
> **Not:** Sunshine Renderer v1.0.0 sürüm hedeflerinin (Faz 0 - 9) başarıyla tamamlanması ve motor çekirdeğinin framework'süz yalın yapısını korumak amacıyla Faz 10 iptal edilmiş ve Gelecek v2.0 Yol Haritasına ertelenmiştir.
> 
> Motor, Faz 9 ile birlikte **v1.0.0 Resmi Sürümü** olarak yayınlanmıştır.

- [x] v1.0.0 Kapsamı Donduruldu (Faz 0 - 9 Tamamlandı).
- [ ] Dear ImGui Vulkan Backend entegrasyonu (v2.0).
- [ ] Sahne Ağacı (Scene Hierarchy) ve Obje İnceleme (Inspector) paneli (v2.0).
- [ ] Işık ve Materyal Ayarlayıcılar (v2.0).
- [ ] Canlı Kamera ve Post-Processing parametre ayarları (v2.0).
- [ ] GPU Bellek ve CPU Frame Time Profiler grafiği (v2.0).

**Durum:** ⏹️ İptal Edildi / v2.0 Yol Haritasına Ertelendi

---

## 🛠️ Temel İlkeler ve Kurallar (Roadmap 2 İçin Geçerli)

1. **Sıfır Vulkan Sızıntısı:** `engine/include/sunshine/` altındaki hiçbir başlık dosyasında `vulkan.h` veya `Vk*` tipi yer alamaz.
2. **Modern C++20:** Temiz sınıflar, `std::span`, `std::unique_ptr`, `std::shared_ptr`, strongly typed enumlar.
3. **Kapsamlı Hata Mesajları:** Tüm yeni arayüzlerde `SUNSHINE_ASSERT` ve bağlamsal `GraphicsException` prensibi sürdürülecektir.
4. **Sürekli İlerleme Takibi:** Her faz tamamlandığında [PROGRESS.md](file:///c:/Users/ibrah/OneDrive/Desktop/Sunshine%20renderer/PROGRESS.md) güncellenerek kayıt altına alınacaktır.
