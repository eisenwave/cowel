import { defineConfig } from "@playwright/test";

const forceVideo = process.env.PLAYWRIGHT_VIDEO === "on";

export default defineConfig({
    testDir: ".",
    testMatch: ["test/ts/watch.test.ts"],
    fullyParallel: false,
    forbidOnly: !!process.env.CI,
    retries: process.env.CI ? 1 : 0,
    reporter: process.env.CI ? [["line"], ["html", { open: "never" }]] : [["list"], ["html"]],
    use: {
        headless: true,
        trace: "retain-on-failure",
        video: forceVideo ? "on" : "retain-on-failure",
    },
    outputDir: "../../build/playwright-results",
});
