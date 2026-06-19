/*
 * LEAP Gateway
 *
 * Copyright (c) 2026 Adam G. Sweeney
 *
 * SPDX-License-Identifier: MIT
 * See the LICENSE file in the repository root for full license text.
 */

var GW_HELP = {
  "index.html": {
    title: "Configuration Console Help",
    html:
      "<p>This is the <b>LEAP Gateway</b> configuration console on the NetBurner MOD5441X module.</p>" +
      "<h3>Current capabilities</h3>" +
      "<ul>" +
      "<li><b>Network Configuration</b>  - Port 1 plant network and Port 2 LEAP network IPv4 settings on MOD54417.</li>" +
      "<li><b>LEAP Device Mappings</b>  - Map LEAP peer MAC addresses to EtherNet/IP assembly byte offsets.</li>" +
      "</ul>" +
      "<h3>EtherNet/IP / PLC setup</h3>" +
      "<p>RSWho discovery only shows the device identity. To see I/O assemblies in Studio 5000:</p>" +
      "<ol>" +
      "<li>Install <b>eds/LEAP_Gateway.eds</b> with the Rockwell EDS Hardware Installation Tool.</li>" +
      "<li>In Studio 5000, add an Ethernet module: catalog <b>LEAP-Gateway</b> (Communications Adapter, product code 65001).</li>" +
      "<li>Use connection <b>Exclusive Owner</b>: Output assembly 150, Input assembly 100, Config assembly 151, RPI 20-30 ms.</li>" +
      "</ol>" +
      "<p>Assemblies are not listed in RSWho; they appear after the module is created in the PLC project.</p>" +
      "<h3>What is saved where?</h3>" +
      "<ul>" +
      "<li>Network settings are stored in the gateway flash memory.</li>" +
      "<li>Changes to network IP require a <b>reboot</b> to take effect.</li>" +
      "</ul>" +
      "<p>Use <b>Connect LEAP</b> after saving mappings to bootstrap owner sessions on Port 2. "
      + "<b>Discover peers</b> scans for devices not yet in your mapping table (skipped while sessions are active).</p>"
      + "<p>Live I/O table shows LEAP digital values and the mapped EtherNet/IP assembly bytes.</p>"
  },

  "mapping.html": {
    title: "LEAP Device Mappings Help",
    html:
      "<p>Configure how each LEAP device is exposed to a PLC through the gateway EtherNet/IP assemblies.</p>" +
      "<h3>EtherNet/IP assemblies</h3>" +
      "<ul>" +
      "<li><b>Input assembly 100</b> (32 bytes)  - Gateway to PLC: LEAP inputs and status bytes.</li>" +
      "<li><b>Output assembly 150</b> (32 bytes)  - PLC to gateway: LEAP output commands.</li>" +
      "<li><b>Config assembly 151</b> (10 bytes)  - Connection configuration.</li>" +
      "</ul>" +
      "<h3>Mapping fields</h3>" +
      "<ul>" +
      "<li><b>Peer MAC</b>  - LEAP device Ethernet address (aa:bb:cc:dd:ee:ff).</li>" +
      "<li><b>Profile ID</b>  - LEAP device profile (default 0x00010001 for 8x8 digital I/O).</li>" +
      "<li><b>Input byte</b>  - Offset in assembly 100 for LEAP inputs to the PLC.</li>" +
      "<li><b>Output byte</b>  - Offset in assembly 150 for PLC outputs to the LEAP device.</li>" +
      "<li><b>Status byte</b>  - Offset in assembly 100 for LEAP comm/status data.</li>" +
      "<li><b>Width</b>  - Digital I/O width in bits (1-16, typically 8).</li>" +
      "<li><b>Enable</b>  - Include this mapping when LEAP sessions are connected.</li>" +
      "</ul>" +
      "<h3>Buttons</h3>" +
      "<ul>" +
      "<li><b>+ Add mapping</b>  - Create a new mapping slot with default byte offsets.</li>" +
      "<li><b>Refresh</b>  - Reload mappings from the gateway.</li>" +
      "<li><b>Save Mappings</b>  - Store mappings in gateway memory.</li>" +
      "<li><b>Clear All</b>  - Remove every mapping.</li>" +
      "</ul>" +
      "<p>Each mapping slot uses unique assembly byte offsets. LEAP peer discovery and flash persistence will be added as the embedded port progresses.</p>"
  },

  "network.html": {
    title: "Network Configuration Help",
    html:
      "<p>Configure IPv4 addressing for each gateway Ethernet port. After changing settings here, use <b>Save &amp; Reboot</b> unless you only saved for later.</p>" +
      "<h3>MOD54417 port roles (fixed)</h3>" +
      "<ul>" +
      "<li><b>Port 1 - Plant Network</b>  - PLC / EtherNet/IP traffic and this configuration web UI.</li>" +
      "<li><b>Port 2 - LEAP Network</b>  - LEAP devices only. LEAP peers must be connected to this port.</li>" +
      "<li>Bridging both Ethernet ports is <b>not supported</b> on this gateway.</li>" +
      "</ul>" +
      "<h3>IPv4 mode (each port)</h3>" +
      "<ul>" +
      "<li><b>DHCP</b>  - The gateway requests an address from your plant DHCP server. Use this on a normal managed network. If no server is present, the device falls back to link-local <b>169.254.x.x</b> (AutoIP) after about 30 seconds.</li>" +
      "<li><b>DHCP w Fallback</b>  - Tries DHCP first; if that fails, uses the static IP, mask, and gateway you enter below.</li>" +
      "<li><b>Static</b>  - Fixed IP at all times. Enter IP, mask, gateway, and DNS as required by your IT group.</li>" +
      "<li><b>Disabled</b>  - Turns IPv4 off on that interface (advanced; rarely needed).</li>" +
      "</ul>" +
      "<h3>Static fields (Static or DHCP w Fallback)</h3>" +
      "<ul>" +
      "<li><b>Static IP</b>  - Address operators and your PC will use to open this web page.</li>" +
      "<li><b>Mask</b>  - Subnet mask (often 255.255.255.0 on a /24 network).</li>" +
      "<li><b>Gateway</b>  - Router to other subnets; use 0.0.0.0 if everything is on one subnet.</li>" +
      "<li><b>DNS 1 / DNS 2</b>  - Optional name servers; 0.0.0.0 is fine if you connect by IP only.</li>" +
      "</ul>" +
      "<h3>Buttons</h3>" +
      "<ul>" +
      "<li><b>Refresh</b>  - Reload current settings from the gateway.</li>" +
      "<li><b>Save to Flash</b>  - Store settings; reboot later to apply network changes.</li>" +
      "<li><b>Save &amp; Reboot</b>  - Save and restart immediately. A wait dialog appears; the page reloads when the gateway is back.</li>" +
      "</ul>" +
      "<h3>Bench / laptop direct cable</h3>" +
      "<p>For Port 1 or Port 2 bench testing, leave mode on DHCP, set your PC adapter to obtain an IP automatically, wait for both sides to get 169.254.x.x addresses, then browse to the gateway AutoIP shown on this page.</p>"
  }
};

function getHelpPageKey() {
  var path = window.location.pathname || "";
  var name = path.substring(path.lastIndexOf("/") + 1);
  if (!name || name === "") {
    return "index.html";
  }
  return name;
}

function ensureHelpModal() {
  if (document.getElementById("pageHelpModal")) {
    return;
  }
  var overlay = document.createElement("div");
  overlay.className = "modal-overlay";
  overlay.id = "pageHelpModal";
  overlay.setAttribute("aria-hidden", "true");
  overlay.innerHTML =
    "<div class=\"modal modal-wide help-modal\" role=\"dialog\" aria-labelledby=\"pageHelpTitle\" aria-modal=\"true\">" +
      "<div class=\"modal-header\">" +
        "<h2 id=\"pageHelpTitle\">Help</h2>" +
        "<button class=\"modal-close\" type=\"button\" onclick=\"hidePageHelp()\" aria-label=\"Close\">&times;</button>" +
      "</div>" +
      "<div class=\"modal-content help-content\" id=\"pageHelpBody\"></div>" +
      "<div class=\"modal-footer actions\">" +
        "<button class=\"button primary\" type=\"button\" onclick=\"hidePageHelp()\">Close</button>" +
      "</div>" +
    "</div>";
  document.body.appendChild(overlay);
  overlay.addEventListener("click", function(evt) {
    if (evt.target === overlay) {
      hidePageHelp();
    }
  });
  document.addEventListener("keydown", function(evt) {
    if (evt.key === "Escape") {
      hidePageHelp();
    }
  });
}

function openPageHelp() {
  ensureHelpModal();
  var key = getHelpPageKey();
  var page = GW_HELP[key];
  if (!page) {
    return;
  }
  document.getElementById("pageHelpTitle").textContent = page.title;
  document.getElementById("pageHelpBody").innerHTML = page.html;
  var modal = document.getElementById("pageHelpModal");
  modal.classList.add("show");
  modal.setAttribute("aria-hidden", "false");
  document.body.style.overflow = "hidden";
}

function hidePageHelp() {
  var modal = document.getElementById("pageHelpModal");
  if (!modal) {
    return;
  }
  modal.classList.remove("show");
  modal.setAttribute("aria-hidden", "true");
  if (!document.querySelector(".modal-overlay.show")) {
    document.body.style.overflow = "";
  }
}

ensureHelpModal();
