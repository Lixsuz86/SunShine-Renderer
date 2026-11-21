# ☀️ Sunshine Renderer — Yol Haritası 3.0 (ROADMAP 3)
## Sunshine Renderer v2.0 Mimarisi ve Yeni Nesil Grafik Teknolojileri

Bu belge, **v1.0.0 resmi sürümüyle** temel motor mimarisi, RHI katmanı, glTF 2.0 PBR, gölge haritalama, DAG tabanlı Render Graph, HDR Bloom, GPU Compute ve çoklu iş parçacıklı komut kaydı başarıyla tamamlanan Sunshine Renderer'ın **üçüncü nesil (v2.0) mimari hedeflerini** tanımlar.

---

### 🎯 Saf Render Sistemi İlkesi
> **Sunshine Renderer monolitik bir oyun motoru veya GUI editörü değildir.**  
> Tıpkı Filament, bgfx veya Diligent Engine gibi; **saf, modüler, taşınabilir ve tavizsiz bir 3D grafik/render sistemidir**.  
> Editör arayüzleri, sahne tasarım araçları ve GUI katmanları bu motoru temel alan istemci uygulamalara ve geliştiricilere bırakılmıştır. Sunshine Renderer'ın yegane odak noktası: **Vulkan'ın saf donanım gücünü en modern C++20 API ergonomisiyle sunmaktır.**

---

## 🧭 Milestone Özeti (Roadmap 3 — v2.0 Hedefleri)

| Faz | Başlık | Hedef Çıktı | Durum |
|:---:|:---|:---|:---:|
| **Faz 10** | Image-Based Lighting (IBL) & HDRI Skybox | `examples/09_ibl_skybox` | ✅ Tamamlandı |
| **Faz 11** | Cascaded Shadow Maps (CSM) & Açık Dünya Gölgeleri | `examples/10_cascaded_shadows` | ✅ Tamamlandı |
| **Faz 12** | GPU-Driven Rendering & Bindless Doku Mimarisi | `examples/11_gpu_driven` | ⬜ Başlanmadı |
| **Faz 13** | Donanım Hızlandırmalı Işın İzleme (Hardware Ray Tracing) | `examples/12_ray_tracing` | ⬜ Başlanmadı |
| **Faz 14** | Ekran Uzayı Yansımaları (Screen-Space Reflections - SSR) | `examples/13_ssr` | ⬜ Başlanmadı |
| **Faz 15** | Zamansal Kenar Yumuşatma (Temporal Anti-Aliasing - TAA) | `examples/14_taa` | ⬜ Başlanmadı |

---

## 📌 Faz Detayları

### Faz 10 — Image-Based Lighting (IBL) & HDRI Skybox
**Hedef:** Sahneye gerçek dünyanın ışık ve yansıma verilerini entegre ederek fotogerçekçi PBR aydınlatma sağlamak.

- [x] **360° Equirectangular HDRI Harita Yükleyici:**
  - Yüksek dinamik aralıklı `.hdr` formatındaki ortam haritalarının okunması ve GPU küp dokusuna (Cubemap) izdüşürülmesi.
- [x] **Diffuse Irradiance Haritası (Hemi-küresel Evrişim):**
  - Compute Shader veya render geçişi ile kosinüs ağırlıklı küresel evrişim (Cosine-weighted convolution) hesaplanması.
- [x] **Specular Pre-filtered Environment Map:**
  - Farklı pürüzlülük (roughness) seviyelerine göre filtrelenmiş yansıma küp haritası (Mipmap piramidi).
- [x] **BRDF İkili Entegrasyon LUT (Split-Sum Approximation):**
  - 2D LUT dokusunda Fresnel-Schlick ve geometri görünürlük integralinin GPU'da önceden hesaplanması.
- [x] **PBR Entegrasyonu:**
  - glTF 2.0 modellerinin hem doğrudan yönlü ışıklardan hem de çevresel HDRI ışığından gerçekçi yansımalar alması.
- [x] **Faz Sonunda:** Farklı pürüzlülükte altın, gümüş, plastik ve cam küreler üzerinde nefes kesici HDRI gökyüzü ve yansımaları.

**Çıktı:** `examples/09_ibl_skybox`

---

### Faz 11 — Cascaded Shadow Maps (CSM) & Açık Dünya Gölgeleri
**Hedef:** Kameraya yakın bölgelerde pikselleşmeyen, ufuk çizgisine kadar uzanan kesintisiz açık dünya gölge sistemi.

- [x] **Kamera Frustum Bölümleme (Cascade Splitting):**
  - Görüş piramidinin pratik/logaritmik derinlik dilimlerine (3-4 Cascade) ayrılması.
- [x] **Işık Uzayı Dönüşümleri & Texel Snapping:**
  - Her dilim için sıkı (tight-fit) ortografik projeksiyon matrisinin hesaplanması.
  - Kamera hareketinde gölge kenarlarının titremesini (shadow shimmering) önleyen piksel hizalama (sub-pixel snapping).
- [x] **2D Texture Array Gölge Tamponu:**
  - Tek bir `VK_IMAGE_VIEW_TYPE_2D_ARRAY` arabelleğinde tüm dilimlerin depolanması.
- [x] **Dilim Harmanlama (Cascade Blend Band):**
  - Dilim geçiş noktalarında ani çözünürlük farkını yumuşatan geçiş harmanlaması.
- [x] **Faz Sonunda:** Geniş bir arazide hem ayak ucundaki mikro detayların hem de kilometrelerce uzaktaki dağların pürüzsüz gölgeleri.

**Çıktı:** `examples/10_cascaded_shadows`

---

### Faz 12 — GPU-Driven Rendering & Bindless Doku Mimarisi
**Hedef:** Çizim çağrısı darboğazını tamamen ortadan kaldırarak milyonlarca poligonu sıfır CPU ek yüküyle GPU üzerinden yönetmek.

- [ ] **Bindless Doku Mimarisi (`VK_EXT_descriptor_indexing`):**
  - Sahnedeki binlerce dokunun tek bir global descriptor dizisine (`sampler2D textures[]`) bağlanması.
  - Materyallerin descriptor seti değiştirmeden doğrudan doku indeksi (`uint textureId`) üzerinden örneklenmesi.
- [ ] **GPU Tabanlı Görüş Alanı Elemesi (Compute Frustum Culling):**
  - Sahnede yer alan tüm nesnelerin sınırlayıcı kürelerinin (Bounding Sphere/AABB) Compute Shader'da kamera frustum düzlemlerine karşı test edilmesi.
- [ ] **Dolaylı Çizim (Indirect Draw):**
  - `vkCmdDrawIndexedIndirect` ve `vkCmdDrawIndexedIndirectCount` desteği.
  - Görünür nesnelerin çizim komutlarının CPU yerine doğrudan Compute Shader tarafından GPU tamponuna yazılması.
- [ ] **GPU Hi-Z Occlusion Culling:**
  - Önceki karenin derinlik piramidini kullanarak diğer nesnelerin arkasında kalan geometrilerin erken elenmesi.
- [ ] **Faz Sonunda:** 50.000+ bağımsız nesnenin tek bir indirect draw çağrısıyla 200+ FPS hızında render edilmesi.

**Çıktı:** `examples/11_gpu_driven`

---

### Faz 13 — Donanım Hızlandırmalı Işın İzleme (Hardware Ray Tracing)
**Hedef:** Modern RTX / RDNA GPU'ların adanmış ışın izleme çekirdeklerini kullanarak fotogerçekçi ışık fiziği elde etmek.

- [ ] **Donanım Desteği Tespiti & Dinamik Başlatma:**
  - `VK_KHR_ray_tracing_pipeline` ve `VK_KHR_acceleration_structure` uzantılarının çalışma anında tespiti.
- [ ] **Hızlandırma Yapıları (Acceleration Structures):**
  - Alt Seviye Hızlandırma Yapısı (BLAS - Bottom-Level Acceleration Structure) inşası.
  - Üst Seviye Hızlandırma Yapısı (TLAS - Top-Level Acceleration Structure) ve dinamik obje dönüşümleri.
- [ ] **Ray Tracing Pipeline & Shader Binding Table (SBT):**
  - Ray Generation, Miss, Closest Hit ve Any-Hit shader modülleri.
- [ ] **Işın İzlemeli Sert ve Yumuşak Gölgeler (Ray Traced Shadows):**
  - Shadow map çözünürlük kısıtlamalarını ve acne sorunlarını aşan fiziksel gerçeklikte gölgeler.
- [ ] **Işın İzlemeli Yansımalar (Ray Traced Reflections):**
  - Yansıtıcı zemin ve metaller üzerinde ekran dışındaki nesneleri de yansıtan kusursuz ayna efektleri.
- [ ] **Faz Sonunda:** Hibrit rasterizasyon + donanım hızlandırmalı ray tracing ile ayna gibi yansıyan nesneler ve kusursuz gölgeler.

**Çıktı:** `examples/12_ray_tracing`

---

### Faz 14 — Ekran Uzayı Yansımaları (Screen-Space Reflections - SSR)
**Hedef:** Donanımsal Ray Tracing desteklemeyen GPU'larda da dinamik yüzey yansımaları elde etmek.

- [ ] **Hi-Z Derinlik Işın Yürütme (Hierarchical Ray Marching):**
  - Fragment shader'da yansıma ışınlarının ekran uzayı derinlik tamponu üzerinde hiyerarşik adımlarla yürütülmesi.
- [ ] **İkili Arama (Binary Search Refinement):**
  - Işın kesişim noktasının hassaslaştırılması ve yapay kırılmaların engellenmesi.
- [ ] **Kenar Sönümleme (Edge Fade) & Pürüzlülük Bulanıklaştırma:**
  - Ekran kenarlarında yansımanın yumuşakça kaybolması ve pürüzlü yüzeylerde yansıma bulanıklığı.
- [ ] **Faz Sonunda:** Islak zemin, su birikintileri ve cilalı metaller üzerinde dinamik ekran uzayı yansımaları.

**Çıktı:** `examples/13_ssr`

---

### Faz 15 — Zamansal Kenar Yumuşatma (Temporal Anti-Aliasing - TAA)
**Hedef:** Yüksek frekanslı kenarlardaki tırtıklanmayı (aliasing) ve piksel titremesini sinematik netlikte ortadan kaldırmak.

- [ ] **Alt-Piksel Kamera Titretme (Sub-pixel Camera Jittering):**
  - Projeksiyon matrisine her kare Halton(2, 3) dizisi ile piksel altı kaydırma uygulanması.
- [ ] **2D Ekran Uzayı Hareket Vektörleri (Motion Vectors):**
  - Önceki kare dünya matrisi ile mevcut kare dünya matrisi farkından dinamik piksel hız vektörlerinin üretilmesi.
- [ ] **Geçmiş Arabelleği Yeniden İzdüşürme (History Reprojection):**
  - Mevcut pikselin önceki karedeki konumunun bulunması ve renklerin birikimli harmanlanması.
- [ ] **Renk Sıkıştırma (Neighborhood Clamping / Color Clipping):**
  - Hareketli nesnelerin arkasında oluşan hayalet izlerin (ghosting artifacts) $3\times3$ piksel komşuluk kutusu ile engellenmesi.
- [ ] **Faz Sonunda:** Sıfır piksel titremesiyle sinematik film kalitesinde kristal netliğinde görüntü.

**Çıktı:** `examples/14_taa`

---

## 🛠️ Temel İlkeler ve Kurallar (Roadmap 3 İçin Geçerli)

1. **Sıfır Vulkan Sızıntısı:** `engine/include/sunshine/` altındaki hiçbir başlık dosyasında `vulkan.h` veya ham `Vk*` tipleri yer alamaz.
2. **Modern C++20 Standardı:** `std::span`, güçlü tip denetimli enumlar, `std::unique_ptr`, `std::shared_ptr` ve RAII prensibi tavizsiz korunur.
3. **Kapsamlı ve Eyleme Geçirilebilir Hata Mesajları:** Kriptik hata kodları yerine dosya, satır ve çözüm önerisi sunan `GraphicsException` mimarisi sürdürülür.
4. **Entegre & Harici GPU Uyumluluğu:** İleri düzey özellikler (örn. Ray Tracing) donanım desteği olmadığında motora çökmeden zarif bir şekilde (graceful fallback) uyum sağlar.
5. **Sürekli İlerleme Takibi:** Her yeni faz tamamlandığında [PROGRESS.md](file:///c:/Users/ibrah/OneDrive/Desktop/Sunshine%20renderer/PROGRESS.md) dosyası düzenli olarak güncellenir ve GitHub'a yedeklenir.
