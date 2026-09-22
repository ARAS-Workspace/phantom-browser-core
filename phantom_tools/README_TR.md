# phantom_tools: geliştirme hattı

Bu ağaç **geliştirme** içindir: IDE, hata ayıklayıcı, test çalıştırma ve hızlı artımlı
döngü burada yaşar. Üretim paketi `phantom-browser` deposunda alınır, üretim
bayrakları da oradaki `config/flags.gn` dosyasında durur. O bayraklar buraya
kopyalanmaz ve iki ağaç aynı build dizinini paylaşmaz.

|              | burası (core)                                                    | phantom-browser (hat)                 |
|--------------|------------------------------------------------------------------|---------------------------------------|
| amaç         | geliştirme, test, hata ayıklama                                  | üretim, CI, paketleme                 |
| bayraklar    | `flags-dev.gn`: DCHECK açık, LTO ve PGO kapalı, `symbol_level=2` | `config/flags.gn`: official, LTO, PGO |
| build dizini | `out/dev`                                                        | `out/arm64`, `out/x64`                |

## Betikler

    phantom_tools/core-sync.sh [verify deps configure compdb]
    phantom_tools/core-build-mac.sh [-j N] [hedef...]
    phantom_tools/core-gate-linux.sh [gen check]

`core-sync.sh` aşamaları:

- **verify** ana makineyi doğrular (Xcode, metal).
- **deps** önce `DEPS` dosyasının sabitlediği depot_tools sürümünü çeker, ardından
  `gclient sync` çalıştırır. İşaretçi (`phantom_tools/.deps/staging/deps-synced-at`) HEAD
  ile eşleşiyorsa ve bağımlılık ağaçları yerindeyse hiçbir şey yapmaz.
- **configure** `out/dev` için `gn gen` çalıştırır; `--fail-on-unused-args` ve
  `--export-compile-commands="$COMPDB_TARGETS"` ile. Bayraklar değişmediyse aşamayı
  atlar.

  `COMPDB_TARGETS` derleme veritabanının hangi hedefleri kapsayacağını belirler ve
  varsayılan değeri `chrome`'dur. Bu sınır olmadan `gn` bildiği her hedef için bir
  komut yazar; hiç çalışmamış testler ve fuzzer'lar da buna dahildir ve IDE o
  hedeflerin üreteceği her dosyayı eksik diye raporlar. Liste `args.gn` dosyasına
  bir yorum satırı olarak yazılır, böylece değiştiğinde `gn gen` yeniden çalışır.
  Bir test ikilisi döngüye katıldığında listeyi genişletin, örneğin
  `"chrome unit_tests"`.
- **compdb** `compile_commands.json` dosyasını ağacın köküne bağlar. IDE'nin kodu
  gerçekten anlaması bu dosyayla gelir.

`IDE_TR.md` dosyası CLion nasıl-yapılır kılavuzudur: projeyi açmadan önce nelerin
ayarlanacağı, projenin nasıl açılacağı ve IDE üzerinden nasıl derleyip hata
ayıklanacağı.

## `.deps/src` neden bir symlink

Chromium'un `DEPS` dosyası yollarını `src/` önekiyle yazar ve `use_relative_paths`
kullanmaz. Bu yüzden gclient'in çözüm adı **`src` olmak zorundadır**. Bu depo farklı
bir dizin adı altında durduğundan `phantom_tools/.deps/src`, ağacın köküne bakan bir
symlink olarak kurulur ve gclient bu bağlantı üzerinden çalışır. `core-sync.sh`
bağlantının gerçekten köke çözüldüğünü doğrular.

`.deps/` yereldir ve depoya işlenmez (`phantom_tools/.gitignore`). gclient'in ağaca
bıraktığı bağımlılık dizinleri, `out/` dizini ve üretilen `compile_commands.json`
dosyası ise bu deponun kendi `.gitignore` dosyasında, onları adlandıran bir blok
altında listelenir. Burası bir fork: upstream bu ağaca birleştirmeyle değil, bizim
yönettiğimiz bir döngüyle giriyor, dolayısıyla o dosya bizim düzenleyebileceğimiz
bir dosya.

## Linux kapısı

Bu ağaç mac için derlenir. Linux derlenmez, **doğrulanır**: mac üzerinde bir
linux link'i chromium'un desteklediği bir yapılandırma değil, ve istediğimiz
değer zaten ona ihtiyaç duymuyor.

    phantom_tools/core-gate-linux.sh gen    # gn gen out/dev-linux, target_os=linux
    phantom_tools/core-gate-linux.sh check  # aynı dizinde gn check

Bayrakları `flags-dev-linux.gn` taşır; `flags-dev.gn`'e dokunulmaz, çünkü o
dosyada `use_system_xcode=false` var ve bu yalnız mac'te anlamlı.
`out/dev-linux` yalnız ninja dosyaları tutar, bir gigabaytın çok altında.

Her silme paketinden önce koşar. `gn gen`, target_os="linux" ile her BUILD.gn'i
okur ve her etiketi çözer; mac kapılarının sessiz kaldığı iki defekt tam burada
göründü: artık arkasında BUILD.gn olmayan bir `//media/gpu/chromeos` etiketi ve
`ui/aura` içindeki koşulsuz bir sources satırı — o dosyayı mac yapılandırması
`use_aura` false olduğu için hiç yüklemiyor.
