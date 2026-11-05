const fs = require("fs");

const html = fs.readFileSync(__dirname + "/ecosystem.html", "utf8");
const engineStart = html.indexOf(">", html.indexOf('<script id="engine"')) + 1;
const engineEnd = html.indexOf("</script>", engineStart);
const engineSource = html.slice(engineStart, engineEnd);

const engine = new Function(engineSource + "return { parseInputFile, simulateOneGeneration, formatOutput };")();

const inputText = fs.readFileSync(0, "utf8");
const input = engine.parseInputFile(inputText);

let world = input.world;
for (let generation = 0; generation < input.numberOfGenerations; generation++) {
  world = engine.simulateOneGeneration(world, generation, input.config);
}

console.log(engine.formatOutput(world, input.config));
