# AI_RULES.md — Sunshine Renderer

Bu dosya, bu proje üzerinde çalışan bir AI asistanının (Claude dahil) uyması
gereken kuralları içerir. Amaç: tutarlı mimari kararlar, framework'süz
prensibine sadakat, ve "OpenGL kolaylığı + Vulkan performansı" vizyonunun
her kod satırında korunması.

---

## 1. Temel Kimlik ve Kapsam

- Bu proje **Sunshine Renderer**: kendi frontend API'sine sahip, Vulkan
  üzerine kurulu, portable bir 3D renderer.
- "Custom backend" = Vulkan'ı alt katman olarak kullanan, kendi isimlendirme
  ve API tasarımına sahip bir sistem. **Gerçek bir GPU sürücüsü değildir** ve
  öyle sunulmamalıdır.
- Dil: **C++** (modern C++, en az C++17; C++20 kullanılabilirse tercih edilir
  ama derleyici/platform uyumluluğu önce kontrol edilir).
- Hedef platformlar: Windows ve Linux öncelikli. macOS, MoltenVK üzerinden
  mümkün ama şu an aktif hedef değil.
- Kod ve commit mesajları İngilizce, sohbet ve açıklamalar Türkçe olabilir
  (kullanıcının tercihine göre — bkz. bölüm 7).

---

## 2. Framework Kullanmama İlkesi

- Vulkan dışında **hazır bir render motoru veya render soyutlama kütüphanesi**
  (bgfx, Diligent Engine, Filament, Sokol, vs.) **kullanılmaz**. Bunların
  yaptığı işi biz kendimiz yazıyoruz — projenin bütün amacı bu.
- **Kabul edilebilir yardımcı kütüphaneler** (renderer'ın kendisi değil,
  altyapı):
  - Pencere/girdi: GLFW veya SDL2 (sadece pencere+input katmanı için)
  - Vulkan fonksiyon yükleme: volk
  - Shader derleme: glslang / shaderc (GLSL → SPIR-V)
  - Görsel yükleme: stb_image
  - Matematik: GLM (matris/vektör matematiğini sıfırdan yazmak proje kapsamı
    dışında sayılır, ama bu konuda kullanıcıyla teyitleşilmeli — bkz. bölüm 8)
- **Şüpheli/gri alan kütüphaneler** (kullanmadan önce kullanıcıya sorulmalı):
  VMA (Vulkan Memory Allocator), ImGui, herhangi bir "render graph" kütüphanesi.
  Bunlar "yardımcı araç" ile "renderer'ın kendisini yazıyorlar" sınırında.
- Bir kütüphane eklemeden önce AI, şu soruyu kendine sormalı: *"Bu, renderer'ın
  temel işlevinin (pipeline, buffer, sync, command yönetimi) bir parçasını mı
  yapıyor, yoksa sadece çevresel bir kolaylık mı sağlıyor?"* İlki ise **yazma**,
  ikincisi ise kullanılabilir ama yine de kullanıcıya haber verilmeli.

---

## 3. API Tasarım Kuralları (Frontend / Sunshine API)

- Sunshine'ın public API'si **Vulkan'ın veya OpenGL'in isimlendirmesini
  birebir kopyalamaz.** Örneğin `vkCreateBuffer` yerine `Sunshine::CreateBuffer`
  değil, projeye özgü, anlamlı isimler tercih edilir (örn. `Device::CreateBuffer`
  gibi kendi mimarisine oturan bir isim — kopya değil, ilham).
- Kullanıcıdan (yani API'yi kullanacak geliştiriciden) **elle yönetilmesi
  istenmeyecek şeyler:**
  - Semaphore/fence senkronizasyonu (frame lifecycle API'si bunu gizler)
  - Descriptor pool/set yaşam döngüsü (otomatik yönetilir)
  - Command buffer allocation/reset detayları
  - Bellek tipi seçimi (`VkMemoryPropertyFlags` gibi düşük seviye detaylar
    sarmalanır, kullanıcı sadece "bu buffer GPU-local mi CPU-visible mi"
    gibi basit bir niyet belirtir)
- **RAII zorunlu.** Hiçbir Vulkan handle'ı elle `vkDestroy*` ile serbest
  bırakılmaz; her kaynak sarmalayıcı sınıfın yıkıcısında (destructor)
  temizlenir.
- Hata yönetimi: Vulkan'ın `VkResult` kodları kullanıcıya asla çıplak
  sızdırılmaz. Faz 1'de sabitlenen kural:
  - Kritik donanım/kurulum/bellek hatalarında: `Sunshine::GraphicsException`
    kullanılır (dosya, satır, fonksiyon ve bağlamsal açıklama içerir).
  - Sıcak yolda (frame döngüsü, resize, minimize durumları): Exception
    fırlatılmaz; `BeginFrame` fonksiyonu `std::optional<FrameContext>`
    döndürür ve sıfır-overhead (zero overhead) garanti edilir.

---

## 4. Performans Kuralları (Backend / Vulkan Katmanı)

- "Vulkan kadar performanslı" iddiası ciddiye alınır: frontend katmanının
  eklediği soyutlama, **runtime'da ölçülebilir overhead'e** (gereksiz kopyalar,
  gereksiz senkronizasyon, gereksiz allocation) yol açmamalı.
- Sıcak yol (hot path — her frame çalışan kod) içinde:
  - Dinamik bellek ayırma (`new`/`malloc`) **olmamalı** — önceden ayrılmış
    havuzlar (pool) kullanılmalı.
  - Gereksiz `vkQueueWaitIdle` / `vkDeviceWaitIdle` gibi tüm GPU'yu durduran
    çağrılardan kaçınılmalı; bunun yerine fence/semaphore tabanlı ince
    senkronizasyon kullanılmalı.
- Pipeline state değişiklikleri mümkün olduğunca gruplanır/cache'lenir
  (Vulkan'da pipeline oluşturmak pahalıdır).
- Her yeni özellik eklenirken AI, "bu entegre GPU'da (Intel UHD) makul
  çalışır mı?" sorusunu göz önünde bulundurmalı — bkz. bölüm 6.

---

## 5. Kod Stili ve Mimari

- Katmanlar net ayrılır: `backend/vulkan/` (ham Vulkan çağrıları burada
  kalır), `engine/rhi/` (soyut arayüzler), `engine/api/` (kullanıcıya açık
  Sunshine API'si). Bu katmanlar birbirine sızmamalı — örneğin `engine/api/`
  içinde asla ham `VkXxx` tipi görünmemeli.
- Her yeni dosya, hangi katmana ait olduğunu net göstermeli (dizin yapısına
  sadık kalınır).
- Yorumlar: *neden* böyle yapıldığı açıklanır, Vulkan'ın *ne* yaptığını
  tekrar anlatan yorumlar yazılmaz (Vulkan dokümantasyonu zaten var).
- Bir fonksiyon/sınıf tasarlarken önce "kullanıcı bunu nasıl çağırmak ister?"
  (API ergonomisi) düşünülür, sonra bu ergonomiyi Vulkan ile nasıl
  gerçekleştireceğimize karar verilir — tersi değil.

---

## 6. Donanım Gerçekliği

- Geliştirme donanımı: Intel UHD entegre grafik (13. nesil Intel Core
  H-series, Casper Nirvana S100 NB 2025). Bu, **birincil test hedefi**.
- Yeni bir özellik önerirken AI, bu özelliğin entegre GPU'da makul şekilde
  çalışıp çalışmayacağını belirtmeli. Ağır teknikler (ray tracing, çok
  büyük bindless texture array'leri, vs.) önerilecekse, bunun entegre
  GPU'da sınırlı/opsiyonel olacağı açıkça söylenmeli.
- Performans hedefleri gerçekçi tutulur: yüksek poligon sayılı AAA sahneler
  değil, orta karmaşıklıkta sahnelerde stabil frame rate hedeflenir.

---

## 7. İletişim ve Çalışma Tarzı

- Kullanıcı (İbrahim) ile teknik tartışmalar Türkçe yürütülür; kod ve teknik
  terimler İngilizce kalabilir (endüstri standardı bu şekilde).
- İbrahim, sistemleri **sıfırdan, öğrenerek inşa etmeyi** tercih ediyor. AI,
  hazır çözüme atlamak yerine, önce yaklaşımı açıklamalı, sonra uygulamalı.
- Roadmap'teki fazlar sırayla takip edilir; bir faz bitmeden bir sonrakine
  ait kod yazılmaz (örneğin RHI soyutlaması olmadan RHI'nin gerektirdiği
  multi-backend kod yazılmaz).

---

## 8. Belirsizlik Durumunda

Aşağıdaki durumlarda AI, varsayım yapıp ilerlemek yerine **kullanıcıya
sormalı**:

- Yeni bir üçüncü parti kütüphane eklenecekse (özellikle bölüm 2'deki
  "gri alan" listesindekiler).
- Bir roadmap fazının kapsamı belirsizse veya fazlar arası atlama
  gerekiyormuş gibi görünüyorsa.
- Performans ile API kolaylığı arasında gerçek bir ödünleşim (tradeoff)
  varsa — bu proje her ikisini de hedeflediği için, hangisinin öncelikli
  olduğu duruma göre değişebilir ve kullanıcı karar vermeli.
- Hedef Vulkan sürümü, minimum donanım gereksinimleri gibi proje çapında
  etkisi olan kararlarda.

---

## 9. Yapılmaması Gerekenler (Kısa Liste)

- ❌ OpenGL veya Vulkan API'lerini birebir taklit eden isimlendirme
- ❌ Hazır render motoru/framework entegrasyonu
- ❌ Sıcak yolda dinamik bellek ayırma
- ❌ Kullanıcıya çıplak `VkResult` veya Vulkan handle'ı sızdırma
- ❌ Roadmap fazlarını atlayarak ileri seviye özellik (PBR, ray tracing vb.)
  önerme
- ❌ Kullanıcıya danışmadan yeni bağımlılık ekleme
