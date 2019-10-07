/**
 * Project:      Open Vehicle Monitor System
 * Module:       Vectrix VX1 Webserver
 *
 * (c) 2019  Anko Hanse <anko_hanse@hotmail.com>
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <sdkconfig.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER


#include <stdio.h>
#include <string>
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_webserver.h"

#include "vehicle_vectrixvx1.h"

using namespace std;

#define _attr(text) (c.encode_html(text).c_str())
#define _html(text) (c.encode_html(text).c_str())


/**
 * WebInit: register pages
 */
void OvmsVehicleVectrixVX1::WebInit()
{
  // vehicle menu:
  //MyWebServer.RegisterPage("/xnl/features", "Features",         WebCfgFeatures,                      PageMenu_Vehicle, PageAuth_Cookie);
  //MyWebServer.RegisterPage("/xnl/battery",  "Battery config",   WebCfgBattery,                       PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xvv/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
  MyWebServer.RegisterPage("/xvv/pwrmon", "Power Monitor", WebPowerMon, PageMenu_Vehicle, PageAuth_Cookie);

}

/**
 * WebDeInit: deregister pages
 */
void OvmsVehicleVectrixVX1::WebDeInit()
{
  //MyWebServer.DeregisterPage("/xnl/features");
  //MyWebServer.DeregisterPage("/xnl/battery");
  MyWebServer.DeregisterPage("/xvv/cellmon");
  MyWebServer.DeregisterPage("/xvv/pwrmon");

}

/**
 * WebCfgFeatures: configure general parameters (URL /xnl/config)
 */
/**
void OvmsVehicleVectrixVX1::WebCfgFeatures(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  bool canwrite;
  std::string modelyear;
  std::string maxgids, maxgids_old;

  if (c.method == "POST") {
    // process form submission:
    modelyear = c.getvar("modelyear");
    maxgids   = c.getvar("maxgids");
    canwrite = (c.getvar("canwrite") == "yes");

    // check:
    if (!modelyear.empty()) {
      int n = atoi(modelyear.c_str());
      if (n < 2011)
        error += "<li data-input=\"modelyear\">Model year must be &ge; 2011</li>";
    }

    if (error == "") {
      // Get old value before we overwrite
      maxgids_old = MyConfig.GetParamValue("xnl", "maxGids", STR(GEN_1_NEW_CAR_GIDS));

      // store:
      MyConfig.SetParamValue("xnl", "modelyear", modelyear);
      MyConfig.SetParamValue("xnl", "maxGids", maxgids);
      MyConfig.SetParamValueBool("xnl", "canwrite", canwrite);

      // Write derived values
      if (maxgids != maxgids_old) {
          if (maxgids == STR(GEN_1_NEW_CAR_GIDS)) {
              MyConfig.SetParamValueInt("xnl", "newCarAh", GEN_1_NEW_CAR_AH);
          }
          else if (maxgids == STR(GEN_1_30_NEW_CAR_GIDS)) {
              MyConfig.SetParamValueInt("xnl", "newCarAh", GEN_1_30_NEW_CAR_AH);
          }
      }

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf feature configuration saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    modelyear = MyConfig.GetParamValue("xnl", "modelyear", STR(DEFAULT_MODEL_YEAR));
    maxgids   = MyConfig.GetParamValue("xnl", "maxGids", STR(GEN_1_NEW_CAR_GIDS));
    canwrite  = MyConfig.GetParamValueBool("xnl", "canwrite", false);

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf feature configuration");
  c.form_start(p.uri);

  c.fieldset_start("General");
  c.input("number", "Model year", "modelyear", modelyear.c_str(), "Default: " STR(DEFAULT_MODEL_YEAR), NULL,
    "min=\"2011\" step=\"1\"", "");

  c.input_radio_start("Battery capacity", "maxgids");
  c.input_radio_option("maxgids", "24 kwh", STR(GEN_1_NEW_CAR_GIDS),    maxgids == STR(GEN_1_NEW_CAR_GIDS));
  c.input_radio_option("maxgids", "30 kwh", STR(GEN_1_30_NEW_CAR_GIDS), maxgids == STR(GEN_1_30_NEW_CAR_GIDS));
  c.input_radio_end("This would change ranges, and display formats to reflect type of battery in the car");
  c.fieldset_end();

  c.fieldset_start("Remote Control");
  c.input_checkbox("Enable CAN writes", "canwrite", canwrite,
    "<p>Controls overall CAN write access, some functions depend on this.</p>");
  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}

*/
/**
 * WebCfgBattery: configure battery parameters (URL /xnl/battery)
 */
/**
void OvmsVehicleVectrixVX1::WebCfgBattery(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  std::string suffrange, suffsoc;

  if (c.method == "POST") {
    // process form submission:
    suffrange = c.getvar("suffrange");
    suffsoc = c.getvar("suffsoc");

    // check:
    if (!suffrange.empty()) {
      float n = atof(suffrange.c_str());
      if (n < 0)
        error += "<li data-input=\"suffrange\">Sufficient range invalid, must be &ge; 0</li>";
    }
    if (!suffsoc.empty()) {
      float n = atof(suffsoc.c_str());
      if (n < 0 || n > 100)
        error += "<li data-input=\"suffsoc\">Sufficient SOC invalid, must be 0…100</li>";
    }

    if (error == "") {
      // store:
      MyConfig.SetParamValue("xnl", "suffrange", suffrange);
      MyConfig.SetParamValue("xnl", "suffsoc", suffsoc);

      c.head(200);
      c.alert("success", "<p class=\"lead\">Nissan Leaf battery setup saved.</p>");
      MyWebServer.OutputHome(p, c);
      c.done();
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(400);
    c.alert("danger", error.c_str());
  }
  else {
    // read configuration:
    suffrange = MyConfig.GetParamValue("xnl", "suffrange", "0");
    suffsoc = MyConfig.GetParamValue("xnl", "suffsoc", "0");

    c.head(200);
  }

  // generate form:

  c.panel_start("primary", "Nissan Leaf battery setup");
  c.form_start(p.uri);

  c.fieldset_start("Charge control");

  c.input_slider("Sufficient range", "suffrange", 3, "km",
    atof(suffrange.c_str()) > 0, atof(suffrange.c_str()), 0, 0, 300, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.input_slider("Sufficient SOC", "suffsoc", 3, "%",
    atof(suffsoc.c_str()) > 0, atof(suffsoc.c_str()), 0, 0, 100, 1,
    "<p>Default 0=off. Notify/stop charge when reaching this level.</p>");

  c.fieldset_end();

  c.print("<hr>");
  c.input_button("default", "Save");
  c.form_end();
  c.panel_end();
  c.done();
}
*/

/**
 * GetDashboardConfig: Vectrix VX1 specific dashboard setup
 */
void OvmsVehicleVectrixVX1::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 50, className: 'green-band' },"
        "{ from: 50, to: 80, className: 'yellow-band' },"
        "{ from: 80, to: 120, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 100, max: 150,"
      "plotBands: ["
        "{ from: 100, to: 110, className: 'red-band' },"
        "{ from: 110, to: 120, className: 'yellow-band' },"
        "{ from: 120, to: 140, className: 'green-band' },"
        "{ from: 140, to: 150, className: 'red-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency: Wh/km
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 45, className: 'green-band' },"
        "{ from: 45, to: 60, className: 'yellow-band' },"
        "{ from: 60, to: 120, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -10, max: 40,"
      "plotBands: ["
        "{ from: -10, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 5, className: 'green-band' },"
        "{ from: 5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 40, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 5, max: 85, tickInterval: 20,"
      "plotBands: ["
        "{ from: 5, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 85, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 15,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 45, className: 'normal-band border' },"
        "{ from: 45, to: 60, className: 'red-band border' }]"
    "},{"
      // Motor Controller temperature:
      "min: 5, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 5, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 100, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 90, className: 'normal-band border' },"
        "{ from: 90, to: 100, className: 'red-band border' }]"
    "}]";
}

/**
 * WebPowerMon: (/xvv/pwrmon)
 */
void OvmsVehicleVectrixVX1::WebPowerMon(PageEntry_t& p, PageContext_t& c)
{
  std::string cmd, output;

  c.head(200);
  PAGE_HOOK("body.pre");

  c.print(
    "<style type=\"text/css\">\n"
    ".metric.number .value { min-width: 5em; }\n"
    ".metric.number .unit { min-width: 3em; }\n"
    ".night .highcharts-color-1 {\n"
      "fill: #c3c3c8;\n"
      "stroke: #c3c3c8;\n"
    "}\n"
    ".night .highcharts-legend-item text {\n"
      "fill: #dddddd;\n"
    "}\n"
    ".highcharts-graph {\n"
      "stroke-width: 4px;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "fill: #000000;\n"
    "}\n"
    ".night .highcharts-tooltip-box {\n"
      "color: #bbb;\n"
    "}\n"
    "#dynochart {\n"
      "width: 100%;\n"
      "max-width: 100%;\n"
      "height: 50vh;\n"
      "min-height: 265px;\n"
    "}\n"
    ".fullscreened #dynochart {\n"
      "height: 80vh;\n"
    "}\n"
    "</style>\n"
    "\n"
    "<div class=\"panel panel-primary panel-single\">\n"
      "<div class=\"panel-heading\">Power Monitor</div>\n"
      "<div class=\"panel-body\">\n"
        "<div class=\"row receiver\" id=\"livestatus\">\n"
          "<div class=\"table-responsive\">\n"
            "<table class=\"table table-bordered table-condensed\">\n"
              "<tbody>\n"
                "<tr>\n"
                  "<th>Speed</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.p.speed\"><span class=\"value\">?</span><span class=\"unit\">kph</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.m.rpm\"><span class=\"value\">?</span><span class=\"unit\">rpm</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.throttle\"><span class=\"value\">?</span><span class=\"unit\">%thr</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.e.footbrake\"><span class=\"value\">?</span><span class=\"unit\">%brk</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Torque</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.trq.limit\"><span class=\"value\">?</span><span class=\"unit\">Nm<sub>lim</sub></span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.trq.demand\"><span class=\"value\">?</span><span class=\"unit\">Nm<sub>dem</sub></span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.trq.act\"><span class=\"value\">?</span><span class=\"unit\">Nm</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Battery</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.vlt.bat\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.current\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"v.b.power\"><span class=\"value\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.vlt.cap\"><span class=\"value\">?</span><span class=\"unit\">V<sub>cap</sub></span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Motor</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.vlt.act\"><span class=\"value\">?</span><span class=\"unit\">V</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.cur.act\"><span class=\"value\">?</span><span class=\"unit\">A</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.pwr.act\"><span class=\"value\">?</span><span class=\"unit\">kW</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.vlt.mod\"><span class=\"value\">?</span><span class=\"unit\">%mod</span></div>\n"
                  "</td>\n"
                "</tr>\n"
                "<tr>\n"
                  "<th>Slip</th>\n"
                  "<td>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.frq.slip\"><span class=\"value\">?</span><span class=\"unit\">rad/s</span></div>\n"
                    "<div class=\"metric number\" data-prec=\"1\" data-metric=\"xvv.i.frq.output\"><span class=\"value\">?</span><span class=\"unit\">rad/s</span></div>\n"
                  "</td>\n"
                "</tr>\n"
              "</tbody>\n"
            "</table>\n"
          "</div>\n"
          "<div id=\"dynochart\"></div>\n"
        "</div>\n"
      "</div>\n"
      "<div class=\"panel-footer\">\n"
        "<form id=\"form-scmon\" class=\"form-horizontal\" method=\"post\" action=\"#\">\n"
          "<div class=\"form-group\">\n"
            "<label class=\"control-label col-sm-3\" for=\"input-filename\">Monitoring control:</label>\n"
            "<div class=\"col-sm-9\">\n"
              "<div class=\"input-group\">\n"
                "<input type=\"text\" class=\"form-control font-monospace\"\n"
                  "placeholder=\"optional recording file path, blank = monitoring only\"\n"
                  "name=\"filename\" id=\"input-filename\" value=\"\" autocapitalize=\"none\" autocorrect=\"off\"\n"
                  "autocomplete=\"section-scmon\" spellcheck=\"false\">\n"
                "<div class=\"input-group-btn\">\n"
                  "<button type=\"button\" class=\"btn btn-default\" data-toggle=\"filedialog\" data-target=\"#select-recfile\" data-input=\"#input-filename\">Select</button>\n"
                "</div>\n"
              "</div>\n"
            "</div>\n"
            "<div class=\"col-sm-9 col-sm-offset-3\" style=\"margin-top:5px;\">\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-start\">Start</button>\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-stop\">Stop</button>\n"
              "<button type=\"button\" class=\"btn btn-default\" id=\"cmd-reset\">Reset</button>\n"
              "<span class=\"help-block\"><p>Hint: you may change the file on the fly (change + push 'Start' again).</p></span>\n"
            "</div>\n"
            "<div class=\"col-sm-9 col-sm-offset-3\">\n"
              "<samp id=\"cmdres-scmon\" class=\"samp\"></samp>\n"
            "</div>\n"
          "</div>\n"
        "</form>\n"
      "</div>\n"
    "</div>\n"
    "\n"
    "<div class=\"filedialog\" id=\"select-recfile\" data-options='{\n"
      "\"title\": \"Select recording file\",\n"
      "\"path\": \"/sd/recordings/\",\n"
      "\"quicknav\": [\"/sd/\", \"/sd/recordings/\"]\n"
    "}' />\n"
    "\n"
    "<script>\n"
    "\n"
    "/**\n"
     "* Form handling\n"
     "*/\n"
    "\n"
    "$(\"#cmd-start\").on(\"click\", function(){\n"
      "loadcmd(\"xvv mon start \" + $(\"#input-filename\").val(), \"#cmdres-scmon\");\n"
    "});\n"
    "$(\"#cmd-stop\").on(\"click\", function(){\n"
      "loadcmd(\"xvv mon stop\", \"#cmdres-scmon\");\n"
    "});\n"
    "$(\"#cmd-reset\").on(\"click\", function(){\n"
      "loadcmd(\"xvv mon reset\", \"#cmdres-scmon\");\n"
    "});\n"
    "\n"
    "/**\n"
     "* Dyno chart\n"
     "*/\n"
    "\n"
    "var dynochart;\n"
    "\n"
    "function get_dyno_data() {\n"
      "var data = {\n"
        "bat_pwr_drv: metrics[\"xvv.s.b.pwr.drv\"],\n"
        "bat_pwr_rec: metrics[\"xvv.s.b.pwr.rec\"],\n"
        "mot_trq_drv: metrics[\"xvv.s.m.trq.drv\"],\n"
        "mot_trq_rec: metrics[\"xvv.s.m.trq.rec\"],\n"
      "};\n"
      "return data;\n"
    "}\n"
    "\n"
    "function update_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart.series[0].setData(data.bat_pwr_drv);\n"
      "dynochart.series[1].setData(data.bat_pwr_rec);\n"
      "dynochart.series[2].setData(data.mot_trq_drv);\n"
      "dynochart.series[3].setData(data.mot_trq_rec);\n"
    "}\n"
    "\n"
    "function init_dyno_chart() {\n"
      "var data = get_dyno_data();\n"
      "dynochart = Highcharts.chart('dynochart', {\n"
        "chart: {\n"
          "type: 'spline',\n"
          "events: {\n"
            "load: function () {\n"
              "$('#livestatus').on(\"msg:metrics\", function(e, update){\n"
                "if (update[\"xvv.s.b.pwr.drv\"] != null\n"
                 "|| update[\"xvv.s.b.pwr.rec\"] != null\n"
                 "|| update[\"xvv.s.m.trq.drv\"] != null\n"
                 "|| update[\"xvv.s.m.trq.rec\"] != null)\n"
                  "update_dyno_chart();\n"
              "});\n"
            "}\n"
          "},\n"
          "zoomType: 'x',\n"
          "panning: true,\n"
          "panKey: 'ctrl',\n"
        "},\n"
        "title: { text: null },\n"
        "credits: { enabled: false },\n"
        "legend: {\n"
          "align: 'center',\n"
          "verticalAlign: 'bottom',\n"
        "},\n"
        "xAxis: {\n"
          "minTickInterval: 5,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "gridLineWidth: 1,\n"
        "},\n"
        "plotOptions: {\n"
          "series: {\n"
            "pointStart: 1,\n"
          "},\n"
          "spline: {\n"
            "marker: {\n"
              "enabled: false,\n"
            "},\n"
          "},\n"
        "},\n"
        "tooltip: {\n"
          "shared: true,\n"
          "crosshairs: true,\n"
          "useHTML: true,\n"
          "headerFormat: '<table>' +\n"
            "'<tr><td class=\"tt-header\">Speed:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.key} kph</b></td></tr>',\n"
          "pointFormat:\n"
            "'<tr><td class=\"tt-color-{point.colorIndex}\">{series.name}:&nbsp;</td>' +\n"
            "'<td style=\"text-align: right\"><b>{point.y}</b></td></tr>',\n"
          "footerFormat: '</table>',\n"
          "valueDecimals: 1,\n"
        "},\n"
        "yAxis: [{\n"
          "title: { text: \"Power [kW]\" },\n"
          "labels: { format: \"{value:.0f}\" },\n"
          "min: 0,\n"
          "minTickInterval: 1,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "showFirstLabel: false,\n"
        "},{\n"
          "title: { text: \"Torque [Nm]\" },\n"
          "labels: { format: \"{value:.0f}\" },\n"
          "min: 0,\n"
          "minTickInterval: 5,\n"
          "tickPixelInterval: 50,\n"
          "minorTickInterval: 'auto',\n"
          "showFirstLabel: false,\n"
          "opposite: true,\n"
        "}],\n"
        "responsive: {\n"
          "rules: [{\n"
            "condition: { maxWidth: 500 },\n"
            "chartOptions: {\n"
              "yAxis: [{\n"
                "title: { text: null },\n"
              "},{\n"
                "title: { text: null },\n"
              "}],\n"
            "},\n"
          "}],\n"
        "},\n"
        "series: [{\n"
          "name: 'Max Drive Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_drv,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Recup Power',\n"
          "tooltip: { valueSuffix: ' kW' },\n"
          "data: data.bat_pwr_rec,\n"
          "yAxis: 0,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Drive Torque',\n"
          "tooltip: { valueSuffix: ' Nm' },\n"
          "data: data.mot_trq_drv,\n"
          "yAxis: 1,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "},{\n"
          "name: 'Max Recup Torque',\n"
          "tooltip: { valueSuffix: ' Nm' },\n"
          "data: data.mot_trq_rec,\n"
          "yAxis: 1,\n"
          "animation: {\n"
            "duration: 300,\n"
            "easing: 'easeOutExpo'\n"
          "},\n"
        "}]\n"
      "});\n"
      "$('#dynochart').data('chart', dynochart).addClass('has-chart');"
    "}\n"
    "\n"
    "/**\n"
     "* Chart initialization\n"
     "*/\n"
    "\n"
    "function init_charts() {\n"
      "init_dyno_chart();\n"
    "}\n"
    "\n"
    "if (window.Highcharts) {\n"
      "init_charts();\n"
    "} else {\n"
      "$.ajax({\n"
        "url: \"" URL_ASSETS_CHARTS_JS "\",\n"
        "dataType: \"script\",\n"
        "cache: true,\n"
        "success: function(){ init_charts(); }\n"
      "});\n"
    "}\n"
    "\n"
    "</script>\n"
  );

  PAGE_HOOK("body.post");
  c.done();
}


#endif //CONFIG_OVMS_COMP_WEBSERVER
