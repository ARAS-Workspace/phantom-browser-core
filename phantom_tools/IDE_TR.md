# CLion kurulumu

CLion'u bu ağaca yöneltmenin adımları. Önce derleyin: `out/dev/gen` altındaki
üretilmiş başlıklar ancak ninja çalıştıktan sonra var oluyor ve onlar olmadan IDE
binlerce include'u çözemiyor.

    phantom_tools/core-sync.sh
    phantom_tools/core-build.sh -j 10

## Projeyi açmadan önce

Bu üç adım ev dizinine dokunuyor.

**1. İndeksleme için dosya boyutu sınırını yükseltin.** Üretilen dosyalar çoğu
zaman 2500 KB olan varsayılan sınırı aşıyor.

    printf 'idea.max.intellisense.filesize=12500\n' \
      >> ~/Library/Application\ Support/JetBrains/CLion2026.2/idea.properties

**2. Bellek.** `clion.vmoptions` dosyası `-Xmx2048m` ile geliyor. En az 8g yapın:

    Help | Edit Custom VM Options   ->   -Xmx8196m

**3. lldb.** Chromium'un yardımcıları kaynak düzeyinde hata ayıklamayı geri
getiriyor ve hata ayıklayıcıya kendi işaretçi, dizge ve vektör türlerini okunabilir
biçimde yazdırmayı öğretiyor.

    cat >> ~/.lldbinit <<'LLDB'
    script sys.path[:0] = ['<ağaç>/tools/lldb']
    script import lldbinit
    script import chromium_visualizers
    LLDB

`<ağaç>` yerine bu çalışma kopyasının mutlak yolunu yazın.

## Projeyi açma

**4.** `File | Open` yolunu izleyin, ağacın kökündeki `compile_commands.json`
dosyasını seçin ve **Open as Project** deyin. Projeyi C++ projesine çeviren adım
budur; dosya üzerinden yapılan bir karşılığı yok.

**5. Hiç okumadığınız yerleri dışarıda bırakın.** Bunu ilk indekslemeden önce
yapın; 499000 dosyanın yaklaşık 235000'ini eler.

    third_party/blink/web_tests   ios   ash   chromeos   android_webview   docs   infra

Project görünümünde seçin, sağ tıklayın, `Mark Directory As`, `Excluded`.
`third_party/blink/renderer` dışarıda bırakılmaz.

## IDE üzerinden derleme ve hata ayıklama

CLion bir derleme veritabanı projesini kendi başına derleyemiyor. Kod analizi ve
tek dosyalık `Recompile` sunuyor; gerisi özel bir hedef üzerinden yürüyor.

**6. Özel derleme hedefi.** `Settings | Build, Execution, Deployment | Custom Build
Targets`. Build alanı bir **External Tool** istiyor, onu yanındaki `...` düğmesiyle
oluşturun:

    Program:           <ağaç>/phantom_tools/core-build.sh
    Argümanlar:        -j 10
    Çalışma dizini:    <ağaç>

Bu araçlar proje bazlı saklanıyor ve `Tools | External Tools` altında görünmüyor.

**7. Çalıştırma yapılandırması.** `Run | Edit Configurations` altında bir **Custom
Build Application** ekleyin, altıncı adımdaki hedefi seçin ve paketi gösterin:

    out/dev/Phantom Browser.app

Hedef `Before launch` alanına yerleşiyor, yani yapılandırmayı çalıştırdığınızda
önce derleme yapılıyor.

**8. Hata ayıklayıcı.** `Settings | Build, Execution, Deployment | Debugger | Debug
Profiles` altında bir LLDB profili oluşturun, sonra araç çubuğundaki seçiciden onu
etkinleştirin. Toolchains sayfasındaki hata ayıklayıcı alanı artık kullanımdan
kalkmış durumda.

## Değiştirmeniz gerekmeyen şey

Toolchains sayfasındaki derleyici alanları algılandığı gibi kalabilir.
`compile_commands.json` içindeki her girdi zaten bu ağacın kendi clang'ını
adlandırıyor, dolayısıyla kod kavrayışı dosya başına gerçek bayrakları okuyor.

## Beklenen davranışlar

- **Dışlanan klasörler görünmeye devam eder.** Dışlama indekslemeyi durdurur,
  klasörü gizlemez. Neyin dışlandığının kaydı `.idea/misc.xml` dosyasındadır.
- **"Found several command objects for file ... Using only one"** iletisi, hem host
  hem hedef toolchain için derlenen dosyalarda çıkar. CLion birini seçer. Bu bir
  bilgi iletisidir, hata değil.
- **Hata ayıklayıcı bir yerel değişkeni "optimized out" gösterebilir.** Bu ağaç
  `-O2` ve `dcheck_always_on=true` ile derleniyor; `symbol_level=2` satır ve tür
  bilgisini koruyor.
- **Yeni bir kaynak dosya veritabanını tazelemez.** `core-sync.sh configure`
  derleme bayraklarını karşılaştırır, kaynak listesini değil. Dosya ekledikten
  sonra `core-sync.sh compdb` çalıştırın.
