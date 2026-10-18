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
| **Faz 7** | İleri Işıklandırma & Gerçek Zamanlı Gölgeler (Shadow Mapping) | `examples/06_shadows` | ⬜ Başlanmadı |
| **Faz 8** | Render Graph & İleri Post-Processing (Bloom / SSAO) | `examples/07_render_graph_bloom` | ⬜ Başlanmadı |
| **Faz 9** | Compute Shader & Multi-threaded Command Recording | `examples/08_gpu_particles` | ⬜ Başlanmadı |
| **Faz 10** | Sunshine Editör & Debug GUI (ImGui Entegrasyonu) | `examples/09_editor` | ⬜ Başlanmadı |

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

- [ ] Sadece derinlik yazan (Depth-Only) Render Pass & Shadow Map Pipeline'ı.
- [ ] Yönlü Işık (Directional Light) için Cascaded Shadow Maps (CSM):
  - Kameranın görüş piramidini (view frustum) dilimlere ayırma.
  - Yakın ve uzak mesafelerde yüksek çözünürlüklü gölge filtreleme.
- [ ] Noktasal Işık (Point Light) için Omnidirectional Cubemap Gölgeleri.
- [ ] PCF (Percentage Closer Filtering) ile donanım destekli yumuşak gölge kenarları (Soft Shadows).
- [ ] Faz Sonunda: Dönen ışık kaynaklarına göre zemine ve nesnelerin üzerine dinamik gölge düşüren sahne.

**Çıktı:** `examples/06_shadows`

---

### Faz 8 — Render Graph & İleri Post-Processing (Bloom / SSAO)
**Hedef:** Çoklu render geçişlerini (pass) ve doku bağımlılıklarını otomatik yöneten modern mimari.

- [ ] Basit ve Güçlü `RenderGraph` / `FrameGraph`:
  - Pass'ler arası görsel bağımlılıkların (dependency) otomatik çözümü.
  - Vulkan `VkImageMemoryBarrier` ve layout geçişlerinin (Color Attachment -> Shader Read) el değmeden otomasyonu.
  - Geçici render target dokularının bellek havuzunda yeniden kullanılması (Transient Memory Aliasing).
- [ ] Yüksek Kaliteli Bloom (Dual-Kawase Blur veya Downsample/Upsample piramidi).
- [ ] SSAO (Screen Space Ambient Occlusion): Nesnelerin köşe ve birleşim yerlerinde gerçekçi ortam gölgelenmesi.
- [ ] Faz Sonunda: Bloom parlamaları ve SSAO ile sinematik görsel kalite.

**Çıktı:** `examples/07_render_graph_bloom`

---

### Faz 9 — Compute Shader & Multi-threaded Command Recording
**Hedef:** GPU genel hesaplama gücünü ve çok çekirdekli CPU paralelizmini motora entegre etmek.

- [ ] Compute Pipeline Arayüzü: `IRHIComputePipeline`, `device.CreateComputePipeline`, `cmd.Dispatch(x, y, z)`.
- [ ] Storage Buffer (SSBO) ve Read/Write Image desteği.
- [ ] GPU Tabanlı Hesaplama Demoları:
  - 100.000+ parçacıklı GPU Parçacık Simülasyonu (Particle Physics)
  - GPU-Driven Frustum Culling (Görüş alanı dışındaki nesnelerin GPU'da elenmesi)
- [ ] Çoklu İş Parçacıklı Komut Kaydı (Multi-threaded Command Recording):
  - Thread havuzu (`WorkerThreadPool`) ile ikincil komut tamponlarının (secondary command buffers) paralel doldurulması.
- [ ] Faz Sonunda: Yüz binlerce parçacığın yüksek kare hızlarında etkileşimi.

**Çıktı:** `examples/08_gpu_particles`

---

### Faz 10 — Sunshine Editör Arayüzü & Debug GUI (ImGui Entegrasyonu)
**Hedef:** Kod derlemeden parametreleri anlık değiştirebilen görsel bir motor editörü.

- [ ] Dear ImGui Vulkan Backend entegrasyonu (Sunshine Command Context ile kesintisiz uyum).
- [ ] Sahne Ağacı (Scene Hierarchy) ve Obje İnceleme (Inspector) paneli.
- [ ] Işık ve Materyal Ayarlayıcılar (Renk tekeri, pürüzlülük/metalik kızakları, ışık konumları).
- [ ] Canlı Kamera ve Post-Processing parametre ayarları.
- [ ] GPU Bellek ve CPU Frame Time Profiler Zaman Çizelgesi grafiği.
- [ ] Faz Sonunda: Bağımsız çalıştırılabilen, modern arayüze sahip Sunshine Sahne Editörü.

**Çıktı:** `examples/09_editor`

---

## 🛠️ Temel İlkeler ve Kurallar (Roadmap 2 İçin Geçerli)

1. **Sıfır Vulkan Sızıntısı:** `engine/include/sunshine/` altındaki hiçbir başlık dosyasında `vulkan.h` veya `Vk*` tipi yer alamaz.
2. **Modern C++20:** Temiz sınıflar, `std::span`, `std::unique_ptr`, `std::shared_ptr`, strongly typed enumlar.
3. **Kapsamlı Hata Mesajları:** Tüm yeni arayüzlerde `SUNSHINE_ASSERT` ve bağlamsal `GraphicsException` prensibi sürdürülecektir.
4. **Sürekli İlerleme Takibi:** Her faz tamamlandığında [PROGRESS.md](file:///c:/Users/ibrah/OneDrive/Desktop/Sunshine%20renderer/PROGRESS.md) güncellenerek kayıt altına alınacaktır.
