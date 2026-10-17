const { DateTime } = require("luxon");
const fs = require("fs");
const path = require("path");

/* ──────────────────────────────────────────────────────────────────
   Book-reader chapter splitter.

   Reads each long-form book from ../docs/ at build time and exposes
   it as an 11ty collection of pages. Each page is one chapter (or
   one appendix / one introduction). The reader UI paginates the
   collection into permalinks like /reader/makers/chapter-3/.

   This keeps the reader.js tiny (just navigation + progress bar)
   because all markdown is rendered to HTML at build time by 11ty.
   ────────────────────────────────────────────────────────────────── */

/* "Chapter 3 — Hello World Oscillator" -> "chapter-3-hello-world-oscillator"
 * "Appendix A — Glossary of Terms"   -> "appendix-a-glossary-of-terms"
 * "Introduction — Why a Book"        -> "introduction-why-a-book"
 * Lower-case, ASCII-folded, hyphen-separated. */
function slugifyHeading(heading, fallback) {
  const s = heading
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "");
  return s || fallback;
}

function splitBook(slug, filePath, opts = {}) {
  const md = fs.readFileSync(filePath, "utf8");
  const lines = md.split("\n");
  const chapters = [];
  let current = null;

  /* Pages are emitted in order: a "front" page (title, ToC, intro),
   * then each chapter / introduction / appendix in document order. */
  for (let i = 0; i < lines.length; i++) {
    const line = lines[i];

    /* New chapter? Start a new page. The maker book splits on
     * "## Chapter"; the music course also has "## Appendix" and
     * "# Introduction" that should each be their own page. */
    const isChapter = /^## Chapter /.test(line);
    const isAppendix = /^## Appendix /.test(line);
    const isIntro = /^# Introduction /.test(line);

    if (isChapter || isAppendix || (isIntro && opts.introAsPage)) {
      if (current) chapters.push(current);
      const raw = line.replace(/^#+\s*/, "");
      const isAppendixPage = isAppendix;
      const isIntroPage = isIntro;
      const kind = isAppendixPage
        ? "appendix"
        : isIntroPage
          ? "introduction"
          : "chapter";
      current = {
        book: slug,
        kind,
        chapterTitle: raw,
        chapterSlug: slugifyHeading(raw, kind),
        chapterIndex: chapters.length,
        content: "",
      };
    } else if (current) {
      current.content += line + "\n";
    } else {
      /* Pre-chapter content: title, subtitle, edition notice, ToC,
       * acknowledgements. Goes onto the "front" page. */
      if (!chapters.find((c) => c.kind === "front")) {
        chapters.push({
          book: slug,
          kind: "front",
          chapterTitle: "Introduction",
          chapterSlug: "front",
          chapterIndex: 0,
          content: "",
        });
      }
      chapters[0].content += line + "\n";
    }
  }
  if (current) chapters.push(current);

  /* Re-index and assign permalink-friendly slugs in order. */
  chapters.forEach((c, i) => {
    c.chapterIndex = i;
    c.permalinkSlug = `${slug}-${i + 1}-${slugifyHeading(c.chapterTitle, c.kind)}`;
  });
  return chapters;
}

module.exports = function (eleventyConfig) {
  /* Pass through binaries & assets that live under public/ */
  eleventyConfig.addPassthroughCopy({ "public": "." });

  /* Pass through CSS + JS from src/assets/. Without this, the CSS/JS
   * the templates reference at /assets/... do not exist in _site and
   * the mobile-nav toggle (and theme toggle) silently fail. */
  eleventyConfig.addPassthroughCopy({ "src/assets/js": "assets/js" });
  eleventyConfig.addPassthroughCopy({ "src/assets/css": "assets/css" });

  /* Pass through static images (the ember hero, etc.) so templates
   * can reference /assets/img/<file> and have the file actually exist
   * in _site/assets/img/. */
  eleventyConfig.addPassthroughCopy({ "src/assets/img": "assets/img" });

  /* Watch CSS/JS/images in dev */
  eleventyConfig.addWatchTarget("src/assets/css/");
  eleventyConfig.addWatchTarget("src/assets/js/");
  eleventyConfig.addWatchTarget("src/assets/img/");

  /* Re-watch book source files so the reader rebuilds when content changes. */
  eleventyConfig.addWatchTarget("../docs/MAKERS_COMPANION.md");
  eleventyConfig.addWatchTarget("../docs/Ravine_Phoenix_Music_Course.md");
  eleventyConfig.addWatchTarget("../docs/DESIGN.md");

  /* Expose each book's chapters as a collection that paginated pages
   * can iterate over. We need both per-book collections (for the
   * book-cover pages) and a unified "allChapters" collection (for
   * the paginated chapter template that emits one URL per chapter
   * across both books).
   *
   * Each chapter is augmented with everything the reader layout
   * needs: title, description, prev/next URLs, all_chapters (for
   * the sidebar), and book metadata. The chapter template (and
   * the reader layout it wraps) just reads from `ch.<field>` --
   * no template-time computation. */
  const repoRoot = path.resolve(__dirname, "..");
  const makersChapters = splitBook(
    "makers",
    path.join(repoRoot, "docs", "MAKERS_COMPANION.md")
  );
  const musicCourseChapters = splitBook(
    "music-course",
    path.join(repoRoot, "docs", "Ravine_Phoenix_Music_Course.md"),
    { introAsPage: true }
  );

  function annotate(bookSlug, chapters, bookMeta) {
    const total = chapters.length;
    /* Build the sidebar list once for this book. */
    const sidebarItems = chapters.map((c) => ({
      title: c.chapterTitle,
      url: "/reader/" + bookSlug + "/" + c.permalinkSlug + "/",
    }));
    return chapters.map((c, idx) => {
      const prevCh = idx > 0 ? chapters[idx - 1] : null;
      const nextCh = idx < total - 1 ? chapters[idx + 1] : null;
      return {
        ...c,
        title: c.chapterTitle,
        description:
          c.chapterTitle +
          " — from the " +
          bookMeta.title +
          " (part of the project reader).",
        book_slug: bookSlug,
        book_title: bookMeta.title,
        book_subtitle: bookMeta.subtitle,
        book_badge: bookMeta.badge,
        book_chapters_total: total,
        book_chapter_index: idx + 1,
        book_current_kind: c.kind,
        book_all_chapters: sidebarItems,
        book_prev: prevCh
          ? {
              title: prevCh.chapterTitle,
              url:
                "/reader/" +
                bookSlug +
                "/" +
                prevCh.permalinkSlug +
                "/",
            }
          : null,
        book_next: nextCh
          ? {
              title: nextCh.chapterTitle,
              url:
                "/reader/" +
                bookSlug +
                "/" +
                nextCh.permalinkSlug +
                "/",
            }
          : null,
      };
    });
  }

  const makersAnnotated = annotate("makers", makersChapters, {
    title: "The Maker's Companion",
    subtitle:
      "A study guide for Make: Electronic Music from Scratch × Ravine: Phoenix.",
    badge: "Hardware × music",
  });
  const musicCourseAnnotated = annotate(
    "music-course",
    musicCourseChapters,
    {
      title: "The PO-33 Musical Companion",
      subtitle: "Beat, melody, groove, and genre for the novice operator.",
      badge: "Music theory",
    }
  );
  eleventyConfig.addCollection("makersChapters", () => makersAnnotated);
  eleventyConfig.addCollection("musicCourseChapters", () => musicCourseAnnotated);
  eleventyConfig.addCollection("allChapters", () => [
    ...makersAnnotated,
    ...musicCourseAnnotated,
  ]);

  /* Short literal for use in templates: ordinalize a number. */
  eleventyConfig.addFilter("ordinal", (n) => {
    const s = ["th", "st", "nd", "rd"];
    const v = n % 100;
    return n + (s[(v - 20) % 10] || s[v] || s[0]);
  });

  /* Find the first object in an array whose key matches the value.
   * Used by the reader chapter template to look up book metadata. */
  eleventyConfig.addFilter("find", (array, key, value) => {
    if (!Array.isArray(array)) return null;
    return array.find((item) => item && item[key] === value) || null;
  });

  /* Markdown filter — wraps markdown-it directly so we can render
   * a markdown string from a Nunjucks template (the chapter page
   * emits one chapter's body this way). */
  const MarkdownIt = require("markdown-it");
  const md = new MarkdownIt({ html: true, linkify: true, typographer: true });
  eleventyConfig.addFilter("markdown", (str) => md.render(str || ""));

  /* Inject id="..." anchors onto every h1–h4 in a rendered markdown
   * string. Used for in-page links like /design/#change-log. We
   * duplicate the slugify rules from the chapter splitter above so
   * heading IDs are predictable across the site. The id is computed
   * from the *inner text* of the heading — we strip the trailing
   * `¶` paragraph-sign markdown-it-anchor would normally inject, and
   * we don't try to disambiguate duplicates (the design doc has no
   * duplicate h2 texts, so collisions don't matter here; if a future
   * doc grows duplicates we can append `-2`, `-3`, etc.). */
  const slugCache = new Map();
  function slugify(text) {
    if (slugCache.has(text)) return slugCache.get(text);
    const s = String(text)
      .toLowerCase()
      .replace(/¶/g, "")
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "");
    slugCache.set(text, s);
    return s;
  }
  function addHeadingAnchors(html) {
    return String(html).replace(
      /<(h[1-4])>([\s\S]*?)<\/\1>/g,
      (m, tag, inner) => {
        /* inner may contain nested HTML (e.g. <code>foo</code>).
         * Build a plain-text version for the slug. */
        const text = String(inner).replace(/<[^>]+>/g, "");
        const id = slugify(text);
        if (!id) return m;
        return `<${tag} id="${id}">${inner}</${tag}>`;
      }
    );
  }
  eleventyConfig.addFilter("anchorHeadings", (html) =>
    addHeadingAnchors(html)
  );

  /* Read a file from disk and return its contents as a string.
   * Used by src/design.njk to pull in docs/DESIGN.md at build time
   * (so we don't have to duplicate the document into a .md template
   * and lose the single-source-of-truth). Path is resolved relative
   * to the project root (one level above website/). */
  const REPO_ROOT = path.join(__dirname, "..");
  eleventyConfig.addFilter("read", (relPath) => {
    const abs = path.isAbsolute(relPath)
      ? relPath
      : path.join(REPO_ROOT, relPath);
    return fs.readFileSync(abs, "utf8");
  });

  /* Same as `read`, but the result is run through the markdown filter
   * and the heading-anchor post-processor. Lets a template do
   * `{{ "docs/DESIGN.md" | readMarkdown | safe }}` in one expression. */
  eleventyConfig.addFilter("readMarkdown", (relPath) => {
    const abs = path.isAbsolute(relPath)
      ? relPath
      : path.join(REPO_ROOT, relPath);
    return addHeadingAnchors(
      md.render(fs.readFileSync(abs, "utf8"))
    );
  });

  return {
    dir: {
      input: "src",
      output: "_site",
      includes: "_includes",
      data: "_data",
    },
    templateFormats: ["njk", "html", "md"],
    htmlTemplateEngine: "njk",
    markdownTemplateEngine: "njk",
  };
};