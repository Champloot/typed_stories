{
  description = "typed_stories: игра на печать (C + raylib, терминальная версия на ncurses)";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      # the mingw-w64 cross compiler, for game.exe
      mingw = pkgs.pkgsCross.mingwW64.buildPackages.gcc;
    in {
      # nix develop .#windows, then: make raylib-win && make windows
      devShells.${system}.windows = pkgs.mkShell {
        nativeBuildInputs = [ mingw pkgs.curl pkgs.unzip pkgs.zip ];
        WIN_CC = "x86_64-w64-mingw32-gcc";
      };

      # nix develop (или direnv с `use flake`) — окружение для make
      devShells.${system}.default = pkgs.mkShell {
        # Инструменты сборки. gcc и make уже входят в mkShell (stdenv),
        # отдельно их добавлять не нужно.
        nativeBuildInputs = with pkgs; [
          pkg-config
          gdb
          clang-tools # clangd для подсказок в Zed (нужен compile_commands.json, см. bear)
          bear        # bear -- make  → compile_commands.json
          # make test: тесты проигрывают целые дни через эмулятор терминала pyte
          (python3.withPackages (ps: [ ps.pyte ]))
        ];

        # Библиотеки: заголовки и .so находит обёртка gcc сама
        # (-I…/include и -L…/lib подставляются автоматически), путь для pkg-config тоже.
        buildInputs = with pkgs; [
          ncurses   # ./game-tty: терминальная версия (отладка, тесты)
          raylib    # ./game: окно (Makefile находит его через pkg-config)
        ];
      };
    };
}
