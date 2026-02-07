{
  description = "Pintos 操作系统实验开发环境";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
          };
        };

        # 统一使用 i686-embedded 交叉工具链，并暴露为 i386-elf-* 命令。
        crossPkgs = pkgs.pkgsCross.i686-embedded;
        crossPrefix = "${crossPkgs.stdenv.targetPlatform.config}-";
        # 使用 nolibc 变体，避免 newlib/libgloss 在某些系统（如 darwin）上构建失败
        crossGcc = crossPkgs.buildPackages.gccWithoutTargetLibc;
        crossBinutils = crossPkgs.buildPackages.binutilsNoLibc;

        # GDB 17.x 在 Apple Clang 下 -Werror 会将 sprintf deprecated、
        # cpu-tic4x.c tautological-overlap-compare 等警告升级为错误，直接关掉即可。
        gdbFixed = pkgs.gdb.overrideAttrs (old: {
          configureFlags = (old.configureFlags or []) ++ [ "--disable-werror" ];
        });

        # 兼容 Pintos 约定的 i386-elf-* 前缀。
        i386Toolchain = pkgs.runCommand "i386-elf-toolchain" {} ''
          mkdir -p "$out/bin"

          for tool in gcc g++ cpp; do
            if [ -x "${crossGcc}/bin/${crossPrefix}$tool" ]; then
              ln -s "${crossGcc}/bin/${crossPrefix}$tool" "$out/bin/i386-elf-$tool"
            fi
          done

          for tool in ld as ar ranlib nm objcopy objdump strip readelf addr2line strings size c++filt; do
            if [ -x "${crossBinutils}/bin/${crossPrefix}$tool" ]; then
              ln -s "${crossBinutils}/bin/${crossPrefix}$tool" "$out/bin/i386-elf-$tool"
            fi
          done

          ln -s "${gdbFixed}/bin/gdb" "$out/bin/i386-elf-gdb"
        '';

        # Pintos 辅助工具 (squish-pty, squish-unix, setitimer-helper)
        pintos-utils = pkgs.stdenv.mkDerivation {
          pname = "pintos-utils";
          version = "1.0";
          src = ./src/utils;

          nativeBuildInputs = [ pkgs.stdenv.cc ];

          buildPhase = ''
            ${pkgs.stdenv.cc}/bin/cc -Wall -W -o setitimer-helper setitimer-helper.c -lm
            ${pkgs.stdenv.cc}/bin/cc -Wall -W -o squish-pty squish-pty.c -lm
            ${pkgs.stdenv.cc}/bin/cc -Wall -W -o squish-unix squish-unix.c -lm
          '';

          installPhase = ''
            mkdir -p $out/bin
            cp setitimer-helper squish-pty squish-unix $out/bin/
          '';

          meta = {
            description = "Pintos 辅助工具";
          };
        };

      in
      {
        devShells.default = pkgs.mkShell {
          # nativeBuildInputs 用于构建时工具
          nativeBuildInputs = with pkgs; [ gnumake ];

          buildInputs = with pkgs; [
            # === 模拟器 ===
            qemu           # QEMU 模拟器 (包含 qemu-system-i386)

            # === 调试工具 ===
            gdbFixed       # GNU 调试器（见上方 darwin/clang 构建修复）
            i386Toolchain  # 统一提供 i386-elf-* 交叉编译工具链

            # === Perl 环境 ===
            perl           # Pintos 脚本是用 Perl 写的

            # === Pintos 辅助工具 ===
            pintos-utils

            # === 其他工具 ===
            coreutils
            findutils
            diffutils
            gawk
            gnugrep
            gnused

            # === 开发辅助 ===
            universal-ctags
            # cgdb         # 更好的 GDB 前端 (可选)
          ];

          # 设置环境变量
          shellHook = ''
            # 添加 Pintos 工具到 PATH (使用当前工作目录)
            export PATH="$PWD/src/utils:$PATH"

            # 设置 PINTOS_HOME 为当前工作目录
            export PINTOS_HOME="$PWD"
            export GCCPREFIX="i386-elf-"

            # GDB 端口 (基于用户 ID)
            export GDBPORT=$(expr $(id -u) % 5000 + 25000)

            # 额外的编译器标志 (用于抑制某些警告，如果需要的话)
            # 如果遇到 format-truncation 错误，可以取消下面这行的注释
            # export EXTRA_CFLAGS="-Wno-format-truncation"

            echo "=========================================="
            echo "  Pintos 开发环境已加载"
            echo "=========================================="
            echo ""
            echo "i386-elf-gcc 版本: $(i386-elf-gcc --version | head -1)"
            echo ""
            echo "可用命令:"
            echo "  pintos            - 运行 Pintos 模拟器"
            echo "  qemu-system-i386  - QEMU 模拟器"
            echo "  gdb               - GNU 调试器"
            echo ""
            echo "快速开始:"
            echo "  cd src/threads && make"
            echo "  pintos -- run alarm-multiple"
            echo ""
            echo "调试示例:"
            echo "  pintos --gdb -- run alarm-multiple"
            echo "  # 然后在另一个终端:"
            echo "  gdb build/kernel.o"
            echo "  (gdb) target remote localhost:$GDBPORT"
            echo ""
            echo "GDB 调试端口: $GDBPORT"
            echo "=========================================="
          '';
        };

        # 也提供一个 packages 输出用于构建工具
        packages = {
          utils = pintos-utils;
          default = pintos-utils;
        };
      }
    );
}
