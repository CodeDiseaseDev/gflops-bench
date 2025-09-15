# Maintainer: Deacon Taylor happyfone579@gmail.com
pkgname=gflops-bench
pkgver=1.0.0
pkgrel=1
pkgdesc="Brutal-mode multithreading/vector benchmark (CPU stress test)"
arch=('x86_64')
url="https://github.com/CodeDiseaseDev/gflops-bench"
license=('MIT')
depends=('glibc')
makedepends=('cmake' 'gcc')
source=("$pkgname-$pkgver.tar.gz::$url/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
  cd "$pkgname-$pkgver"
  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build
}

package() {
  cd "$pkgname-$pkgver"
  install -Dm755 build/gflops-bench "$pkgdir/usr/bin/gflops-bench"
}
