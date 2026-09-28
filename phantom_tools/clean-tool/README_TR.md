# clean-tool

`core-graph.sh` bir çıktı dizininin ninja grafiği üzerinde çalışır. Tek soru gibi
görünen ama tek olmayan iki soruyu cevaplar, ve iki cevabın hiçbirinin build
dizininden gelmesi gerekmeyecek şekilde kendine ait bir ölçüm dizini kurabilir.

## İki ayrı bayatlık

**Artık ninja dosyaları.** gn gen hedef başına bir `.ninja` dosyası yazar ve
kaybolmuş bir hedefin dosyasını hiçbir zaman kaldırmaz. Kümenin tam bir tanımı
var: çıktı dizininin taşıdığı `.ninja` dosyalarından, `build.ninja`'nın
`subninja` ve `include` üzerinden, geçişli olarak ulaşamadıkları. gn gen bunları
temizlemez; `out/dev` içinde o dosyalar oradayken gn gen koştu ve hepsi yerinde
kaldı.

Bir taramayı çarpıtırlar. "Herhangi bir ninja kenarı bu kaynağı adlandırıyor mu"
diye soran bir tarama onları da okur, dolayısıyla geriye kalan tek anılma yeri
ulaşılamayan bir dosya olan bir kaynak canlı okunur. `out/dev` üzerinde ölçüldü:
13469 dosyanın 761'i artık, ve **2476** kaynak yolunu ulaşılan hiçbir dosya
adlandırmıyor ve ağaç da artık taşımıyor.

**Eski grafik.** Bir dizin sıfır artık taşıyıp yine de yolundan çıkmış bir ağacı
anlatabilir, çünkü dosyalarının her biri kaynaklar gitmeden önce yazılmıştır.
`out/c2check` bunun örneği: 0 artık, ve ulaşılan dosyalarının adlandırdığı
kaynaklardan 4143'ü diskte değil.

`measure` ikisini de bildirir, böylece "bu grafik temiz" cümlesi doğru olan için
söylenebilir.

## Build dizini neden temizlenmez

`gn clean` çıktı dizininin `args.gn` dışındaki içeriğini siler, bu da nesneleri
ve çerçeveyi beraberinde götürür. Çerçeve, silme işinin karşılaştırma yaptığı
tanıktır, dolayısıyla bu yanlış bir takas olur.

`ninja -t cleandead` yanlış takastan daha kötüsüdür. `out/dev` içinde
`sdk/xcode_links/` dizinini yürür ve macOS SDK'sının kendi içindeki yollara
`remove()` çağırır; bu makinede onu yalnızca salt okunur SDK durdurdu, ve her
artık `.ninja` dosyasını yerinde bıraktı. Burada koşturulmaz.

## Komutlar

    core-graph.sh measure <out-adi>    yalnız okur, build sürerken de güvenli
    core-graph.sh fresh   <out-adi>    boş bir dizine gn gen
    core-graph.sh prune   <out-adi>    `--write` verilmedikçe kuru koşum
    core-graph.sh selftest             on vaka

`fresh` ve `prune`, `out/dev` ile `out/dev-linux`'u adıyla reddeder, çünkü o
dizinlerin sahibi `core-sync.sh` ve `core-gate-linux.sh`. `prune` bir sembolik
bağı, `sdk/` altındaki hiçbir şeyi ve `.ninja` olmayan bir dosyayı kaldırmaz, ve
her kapıyı kaldırma anında bir kez daha denetler.

## Sınav

On vaka, ve her biri kendisine denk düşen bir enjeksiyon altında kırmızıya döner:
ulaşılan kapanışın `subninja` yanında `include`'u da izlemesi, artık kümesi,
vakanın boş olmaması, sembolik bağ kapısı, `sdk/` kapısı, yalnızca üç kapıyı da
geçen dosyanın kaldırılabilir olması, referans iğnesinin include bayraklarının
göreli parçaları yerine kaynakları alması, daha derin bir göreli referansın bir
ağaç yolu olarak okunmaması, yokluğun bir git indeksi yerine dosya sistemine
sorulması, ve kuru bir koşumun hiçbir şey yazmaması.

Son ikisi orada, çünkü ikisi de başta yanlıştı. Referans iğnesi
`constants.java`'yı `constants.java.tmpl` içinden alıyordu ve `-I../../..`'yı bir
yol olarak okuyordu, ve yokluk testi `git ls-files`'a soruluyordu, ki o 261
gitlink'in içeriğini de `build/mac_files`'ı da listelemez: dosya sisteminin 227
bildirdiği yerde 32452 canlı referansı eksik bildirdi.
