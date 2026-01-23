# Preparing for Masternodes

A beginner-friendly guide to setting up your Mynta wallet on testnet and registering your first masternode.

---

## Who is This Guide For?

This guide is designed for:
- Windows users who are new to cryptocurrency
- People who want to test masternode operation before using real funds
- Anyone who needs help finding or creating configuration files

**No prior experience required** — we'll explain every step.

---

## Table of Contents

1. [Understanding Testnet](#understanding-testnet)
2. [Installing the Mynta Wallet](#installing-the-mynta-wallet)
3. [Finding Your Data Directory](#finding-your-data-directory)
4. [Creating the Configuration File](#creating-the-configuration-file)
5. [Launching on Testnet](#launching-on-testnet)
6. [Getting Testnet Coins](#getting-testnet-coins)
7. [Registering a Masternode](#registering-a-masternode)
8. [Verifying Your Masternode](#verifying-your-masternode)
9. [Common Issues & Solutions](#common-issues--solutions)
10. [Glossary](#glossary)

---

## Understanding Testnet

### What is Testnet?

**Testnet** is a practice version of the Mynta network. It works exactly like the real network (mainnet), but uses coins that have no real value. Think of it like a flight simulator for pilots — all the controls work the same, but there's no risk.

### Why Use Testnet?

| Benefit | Explanation |
|---------|-------------|
| **No Risk** | Testnet coins are free and worthless — mistakes cost nothing |
| **Learn Safely** | Practice masternode setup without risking real money |
| **Test Features** | Try new commands and settings in a safe environment |
| **Debug Issues** | If something goes wrong, you can restart fresh |

> 💡 **Tip**: Always test on testnet first before doing anything on mainnet with real coins!

---

## Installing the Mynta Wallet

### Step 1: Download the Wallet

1. Visit the official Mynta releases page
2. Download the Windows installer (`mynta-qt-win64-setup.exe` or similar)
3. Run the installer and follow the prompts

### Step 2: First Launch

When you first open the Mynta wallet:

1. It will ask where to store blockchain data
2. **Use the default location** (we'll explain this next)
3. The wallet will begin synchronizing with the network

> ⏳ **Note**: Initial sync can take several hours. You can continue with configuration while it syncs.

---

## Finding Your Data Directory

The **data directory** is a folder on your computer where Mynta stores:
- Your wallet file (containing your coins)
- The blockchain data
- Configuration files

### Where is It Located?

On Windows, the default data directory is:

```
C:\Users\YOUR_USERNAME\AppData\Roaming\Mynta
```

### How to Find It (Step-by-Step)

#### Method 1: Using Windows Search

1. Press `Win + R` on your keyboard (opens the Run dialog)
2. Type `%appdata%\Mynta` and press Enter
3. A folder window will open — this is your data directory!

#### Method 2: Using File Explorer

1. Open **File Explorer** (the folder icon on your taskbar)
2. Click in the address bar at the top
3. Type `%appdata%\Mynta` and press Enter

#### Method 3: Manual Navigation

1. Open **File Explorer**
2. Click **View** in the menu bar
3. Check the box for **Hidden items**
4. Navigate to: `C:\Users\YOUR_USERNAME\AppData\Roaming\Mynta`
   - Replace `YOUR_USERNAME` with your Windows username

### What's Inside the Data Directory?

| File/Folder | Purpose |
|-------------|---------|
| `wallet.dat` | Your wallet (contains your coins — BACK THIS UP!) |
| `mynta.conf` | Configuration file (may not exist yet) |
| `blocks/` | Blockchain data |
| `chainstate/` | Blockchain state |
| `debug.log` | Log file for troubleshooting |

> ⚠️ **Important**: If you don't see a `mynta.conf` file, don't worry — we'll create one in the next section!

---

## Creating the Configuration File

The configuration file (`mynta.conf`) tells the Mynta wallet how to operate. For testnet and masternode operation, we need to create and edit this file.

### Step 1: Close the Wallet

If the Mynta wallet is running, close it completely:

1. Click `File` → `Exit` (not just the X button)
2. Wait a few seconds for it to fully shut down

### Step 2: Open the Data Directory

Use one of the methods from the previous section to open your data directory.

### Step 3: Create the Configuration File

#### If `mynta.conf` Doesn't Exist:

1. **Right-click** in an empty area of the folder
2. Select **New** → **Text Document**
3. Name it `mynta.conf` (make sure to remove the `.txt` extension)

> 💡 **Windows Tip**: If you can't see file extensions:
> 1. In File Explorer, click **View**
> 2. Check **File name extensions**
> 3. Now rename the file to remove `.txt`

#### If `mynta.conf` Already Exists:

1. Right-click on `mynta.conf`
2. Select **Open with** → **Notepad**

### Step 4: Add Testnet Configuration

Copy and paste the following into your `mynta.conf` file:

```ini
# ===========================================
# Mynta Testnet Configuration
# ===========================================

# Enable testnet mode
testnet=1

# Allow the wallet to run as a server (required for RPC commands)
server=1

# RPC credentials (change these to something unique!)
rpcuser=myusername
rpcpassword=mysecurepassword123

# Allow RPC connections from your own computer only
rpcallowip=127.0.0.1

# Enable transaction indexing (helpful for some operations)
txindex=1
```

### Step 5: Save the File

1. Click **File** → **Save**
2. Close Notepad

### Understanding the Configuration

| Setting | What It Does |
|---------|--------------|
| `testnet=1` | Connects to the test network instead of mainnet |
| `server=1` | Enables RPC commands (needed for masternode registration) |
| `rpcuser` | Username for RPC authentication |
| `rpcpassword` | Password for RPC authentication |
| `rpcallowip=127.0.0.1` | Only allows commands from your own computer (security) |
| `txindex=1` | Indexes all transactions (useful for troubleshooting) |

> 🔒 **Security Note**: Change `rpcuser` and `rpcpassword` to your own unique values. Never share these!

### Network Port Reference

| Network | P2P Port | RPC Port |
|---------|----------|----------|
| **Mainnet** | 8770 | 8766 |
| **Testnet** | 18770 | 18766 |
| **Regtest** | 18444 | 18443 |

---

## Launching on Testnet

### Step 1: Start the Wallet

Double-click the Mynta wallet icon to launch it.

### Step 2: Verify You're on Testnet

You should see one of these indicators:

- The window title says "Mynta Core - Wallet [testnet]"
- The splash screen shows "testnet"
- The network icon shows a different color

### Step 3: Wait for Sync

The wallet needs to download the testnet blockchain. This is separate from the mainnet blockchain and is much smaller.

You can check sync progress:
1. Look at the progress bar at the bottom of the wallet
2. Or go to **Help** → **Debug Window** → **Information** tab
3. Compare "Blocks" to "Headers" — when they match, you're synced

> ⏳ Testnet sync is usually much faster than mainnet — typically under an hour.

---

## Getting Testnet Coins

Testnet coins are free! You can get them from a "faucet" (a service that gives out test coins).

### Option 1: Community Faucet

1. Get a testnet receiving address:
   - In your wallet, click **Receive**
   - Click **Create new receiving address**
   - Copy the address (starts with 'm' or 'n' on testnet)

2. Visit the Mynta testnet faucet (check Discord or community channels for the current URL)

3. Paste your address and request coins

### Option 2: Mining on Testnet

Testnet mining is much easier than mainnet:

1. Enable mining in your config:
   ```ini
   gen=1
   genproclimit=1
   ```

2. Restart your wallet
3. The wallet will slowly mine testnet coins

### Option 3: Ask the Community

Join the Mynta Discord or community channels and ask for testnet coins. Most community members are happy to send some for testing purposes.

---

## Registering a Masternode

Now for the exciting part — setting up your masternode! 

### Network-Specific Collateral Requirements

| Network | Collateral Required | Confirmations Needed | MN Activation Height |
|---------|---------------------|----------------------|----------------------|
| **Mainnet** | 100,000 MYNTA | 15 blocks (~15 min) | Block 50,000 |
| **Testnet** | 1,000 tMYNTA | 6 blocks (~6 min) | Block 100 |
| **Regtest** | 100 MYNTA | 1 block | Block 1 |

> 💡 **Testnet uses lower amounts** to make testing easier. You only need **1,000 tMYNTA** on testnet!

> ⚠️ **Note**: Masternodes can only be registered and receive payments after the network reaches the activation height. On testnet, this is block 100.

### Prerequisites Checklist

Before starting, make sure you have:

- [ ] Wallet fully synced on testnet
- [ ] At least 1,000 testnet coins (plus a small amount for fees)
- [ ] The Debug Console open (**Help** → **Debug Window** → **Console** tab)

### Step 1: Create the Collateral Transaction

The **collateral** is the MYNTA that proves you're committed to running a masternode. This stays in your wallet — it's not sent anywhere.

**On testnet, you need exactly 1,000 tMYNTA** (on mainnet it's 100,000 MYNTA).

In the Debug Console, type:

```
getnewaddress "masternode_collateral"
```

This creates a new address. **Copy the result** — you'll need it.

Next, send exactly 1,000 coins to this address (for testnet):

```
sendtoaddress YOUR_NEW_ADDRESS 1000
```

Replace `YOUR_NEW_ADDRESS` with the address you just created.

> ⚠️ **Critical**: The amount must be **exactly** 1,000 for testnet (or 100,000 for mainnet) — not 999.99, not 1001.

### Step 2: Wait for Confirmations

The collateral needs confirmations before you can register:
- **Testnet**: 6 confirmations (~6 minutes)
- **Mainnet**: 15 confirmations (~15 minutes)

Check the transaction status:

```
gettransaction YOUR_TRANSACTION_ID
```

Look for `"confirmations": 6` or higher (for testnet).

> 💡 On testnet with 1-minute blocks, this takes about 6 minutes.

### Step 3: Find the Collateral Details

You need two pieces of information:

**Find the transaction ID:**
```
listtransactions
```

Look for the 1,000 MYNTA transaction (testnet) and note the `txid`.

**Find the output index:**
```
gettransaction YOUR_TRANSACTION_ID
```

Look in the `"details"` section for the output with `"amount": 1000` (testnet). Note the `"vout"` number (usually 0 or 1).

### Step 4: Generate Required Addresses

Create addresses for different masternode functions:

```
getnewaddress "masternode_owner"
```

```
getnewaddress "masternode_voting"
```

```
getnewaddress "masternode_payout"
```

**Write down all three addresses!**

### Step 5: Generate BLS Keys

BLS keys are special cryptographic keys used by your masternode to sign messages.

```
bls generate
```

This outputs something like:

```json
{
  "secret": "abc123...64_hex_characters...789xyz",
  "public": "def456...96_hex_characters...uvw012",
  "pop": "ghij78...192_hex_characters...klmn90"
}
```

**Save all three values!** You'll need:
- The **secret** key (64 hex chars) for your masternode configuration AND registration
- The **public** key (96 hex chars) for registration
- The **pop** (Proof of Possession, 192 hex chars) can be used instead of the secret in registration

> 🔒 **Security Note**: The secret key is extremely sensitive. Anyone with this key can operate your masternode. Store it securely!

### Step 6: Register the Masternode

For testnet, you can run the masternode on your own computer. Use `127.0.0.1` as the IP address.

Put it all together with the registration command:

```
protx register COLLATERAL_TXID COLLATERAL_INDEX "127.0.0.1:18770" "OWNER_ADDRESS" "BLS_PUBLIC_KEY" "VOTING_ADDRESS" OPERATOR_REWARD "PAYOUT_ADDRESS" "BLS_SECRET_KEY"
```

Replace the placeholders:

| Placeholder | Replace With |
|-------------|--------------|
| `COLLATERAL_TXID` | Transaction ID from Step 3 |
| `COLLATERAL_INDEX` | Output index (vout) from Step 3 |
| `OWNER_ADDRESS` | Address from "masternode_owner" |
| `BLS_PUBLIC_KEY` | Public key from Step 5 (96 hex characters) |
| `VOTING_ADDRESS` | Address from "masternode_voting" |
| `OPERATOR_REWARD` | Percentage for operator (0-100, usually 0) |
| `PAYOUT_ADDRESS` | Address from "masternode_payout" |
| `BLS_SECRET_KEY` | Secret key from Step 5 (64 hex characters) |

> 💡 **Note**: For testnet, use port `18770`. For mainnet, use port `8770`.

**Example command (with fake values):**

```
protx register abc123def456789... 0 "127.0.0.1:18770" "mABCowner123..." "8f4a2b3c4d5e6f7a8b9c..." "mXYZvoting456..." 0 "mPayout789..." "1a2b3c4d5e6f7a8b9c0d..."
```

> ⚠️ **Important**: The command requires 9 arguments. The 9th argument is your BLS **secret** key, which proves you control the operator key.

### Step 7: Configure for Masternode Operation

Add these lines to your `mynta.conf`:

```ini
# Masternode settings
masternode=1
masternodeblsprivkey=YOUR_BLS_SECRET_KEY
```

Replace `YOUR_BLS_SECRET_KEY` with the **secret** key from Step 5 (64 hex characters).

> ⚠️ **Important**: The option is `masternodeblsprivkey` (not `masternodeprivkey`). This must match the operator public key you registered with.

### Step 8: Restart the Wallet

1. Close the wallet completely (**File** → **Exit**)
2. Reopen the wallet
3. Wait for it to sync

---

## Verifying Your Masternode

### Check Masternode Status

In the Debug Console:

```
masternode status
```

You should see status indicating your masternode is registered and waiting.

### Check the Masternode List

```
masternode list
```

Your masternode should appear in the list.

### Check for Payment Queue Position

```
masternode count
```

This shows how many masternodes are active. Your payments will come approximately every (masternode count) minutes.

### Monitor Your Masternode

To see detailed information:

```
protx list
```

Find your entry and note the `proTxHash`. Then:

```
protx info YOUR_PROTX_HASH
```

Look for:
- `"state"` → `"PoSePenalty": 0` (good!)
- `"confirmations"` should be increasing

---

## Common Issues & Solutions

### "I can't find my data directory"

Make sure hidden files are visible:
1. Open File Explorer
2. Click **View** → Check **Hidden items**
3. Try navigating to `%appdata%\Mynta` again

### "The wallet won't start on testnet"

Check your `mynta.conf`:
- Make sure `testnet=1` is on its own line
- No extra spaces before or after
- Save the file and restart

### "I get 'bad-protx-collateral-amount' error"

Your collateral transaction isn't the exact required amount:
- **Testnet**: Must be exactly 1,000 tMYNTA
- **Mainnet**: Must be exactly 100,000 MYNTA

Create a new transaction with the exact amount for your network.

### "I get 'bad-protx-collateral-immature' error"

Wait for the required confirmations:
- **Testnet**: 6 confirmations
- **Mainnet**: 15 confirmations

Check with:
```
gettransaction YOUR_TX_ID
```

### "My masternode shows as POSE_BANNED"

This means the masternode was offline too long. Fix the issue, then submit:
```
protx update_service YOUR_PROTX_HASH "127.0.0.1:18770"
```

### "RPC commands don't work"

Check that:
1. `server=1` is in your config
2. `rpcuser` and `rpcpassword` are set
3. You've restarted the wallet after changing the config

### "I can't connect to testnet"

- Check your internet connection
- Verify `testnet=1` is in your config
- Try adding testnet seed nodes to your config (ask in community channels)

---

## Glossary

| Term | Definition |
|------|------------|
| **Blockchain** | A digital ledger that records all transactions |
| **BLS Keys** | Special cryptographic keys used for masternode operations |
| **Collateral** | The MYNTA locked to prove masternode commitment (1,000 on testnet, 100,000 on mainnet) |
| **Configuration File** | A text file that controls how the wallet operates |
| **Console** | A text interface for entering commands |
| **Data Directory** | The folder where Mynta stores all its data |
| **Faucet** | A service that gives out free testnet coins |
| **Mainnet** | The real Mynta network with actual value |
| **Masternode** | A special server that provides enhanced network services |
| **PoSe** | Proof of Service — the system that monitors masternode uptime |
| **ProTx** | A special transaction that registers or updates a masternode |
| **RPC** | Remote Procedure Call — a way to send commands to the wallet |
| **Testnet** | A practice network for testing without real value |
| **UTXO** | Unspent Transaction Output — essentially a "coin" in your wallet |

---

## Next Steps

Once you're comfortable with testnet:

1. **Practice** — Try different masternode operations
2. **Monitor** — Watch your masternode payments come in
3. **Learn** — Experiment with other RPC commands
4. **Prepare** — When ready, set up on mainnet with real MYNTA

### Additional Resources

- [Masternode Economics & Rewards](MASTERNODE_GUIDE.md) — Understanding earnings
- [Operator Guide](OPERATOR_GUIDE.md) — Advanced technical setup
- [MIP-001 Specification](MIP-001-DETERMINISTIC-MASTERNODES.md) — Technical details

---

## Support & Community

If you get stuck:

- **Discord**: Join the Mynta community for help
- **GitHub Issues**: Report bugs or documentation issues
- **Community Forums**: Ask questions and share experiences

---

*Last Updated: January 2026*
*Guide Version: 1.0.0*
