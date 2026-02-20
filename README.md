# TrackmaniaPathFinder

Program made to help find paths/reroutes in Trackmania maps. The recommended workflow for finding a path is:

1. Find all of the possible ways you can get from each CP to every other CP and write down in a spreadsheet the time for each such connection, given the best realistically possible entry speed/angle.
2. Run this program to find all of the fastests possible routes that use those connections.
3. (Optional) Adjust connection times used in the fastests routes to account for time penalty based on what CP came before that connection (see [sequence dependence](#sequence-dependence)). You can use [verified connections list](#verified-connections-list) feature to help with this process. Go back to Step 2.
4. Among the routes found choose the best one - fastest on paper doesn't mean the best. Because of inevitable inaccuracies of measuring connection times sometimes it's not the TOP 1 solution that is actually the fastests. Additionally some routes can be much easier to drive than others.

The following sections explain how to use this program. It's written more as a practical guide, rather than full technical specification. I do include some information on implementation details at the end for those who are interested.

If you have any questions / feature requests the best way is to contact me through discord [@isfoo](https://discordapp.com/users/552077071333982219 "@isfoo")

## Table of Contents

- [Program screenshot](#program-screenshot)
- [Input spreadsheet](#input-spreadsheet)
    - [Sequence dependence](#sequence-dependence)
    - [Verified connections list](#verified-connections-list)
    - [Google spreadsheet template](#google-spreadsheet-template)
- [Program usage](#program-usage)
    - [Basic path finding](#basic-path-finding)
    - [Advanvced path finding](#advanced-path-finding)
    - [Output format](#output-format)
    - [Connection finder mode](#connection-finder-mode)
    - [Other tabs](#other-tabs)
- [Examples of real data used for reroutes](#examples-of-real-data-used-for-reroutes)
- [Implementation details](#implementation-details)
    - [Exact algorithm](#exact-algorithm)
    - [Heuristic algorithm](#heuristic-algorithm)

## Program screenshot

<img width="1920" height="1007" alt="tmPathFinder_screenshot" src="https://github.com/user-attachments/assets/c0761de5-8182-4ee8-9e78-2e331d13747c" />

## Input spreadsheet

First you need to create spreadsheet containing N by N cells, where N is the number of CPs + 1. So if I have map that has Start, CP1, CP2, CP3, CP4, Finish I have 5x5 table:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | 999       | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | 999     | 999     | 999     | 999      |
| **CP3**     | 999       | **999**     | 999     | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | 999     | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | 999      |

You can use [google spreadsheet template](#google-spreadsheet-template) described later to automate this process.

The row specifies from where you're going and the column specifies where you're going to. So for example in the above table the bolded cell will contain information about how long does it take to go from CP1 to CP3.

Initially the table should be filled with some big number, like that 999 in this example. This will mean that those connections take 999 seconds. This arbitrary big value will be used to specify that given connection is not possible / should not be considered.

We can start filling in the table with the data we collected. Notice that the diagonal represents the normal route - going from Start to CP1, from CP1 to CP2, etc. Lets fill those in:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | **8.8**      | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | **15**     | 999     | 999     | 999      |
| **CP3**     | 999       | 999     | **12.5**    | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | **16.3**    | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | **25**       |

Now we can fill the rest. Of course you only need to fill as much as you want or can - after all some connections might not even be possible. 

Note that in practice I would recommend inputting the best possible times for each connection. That is you should assume the best possible sequence of previous CPs leading to this connection and measuring that optimal time. If the connection is hard to drive and measure accurately I would recommend to input the lowest possible time you think the connection might take. You can always go back and re-measure the connection if it happens to show up in the fastests route, while if you over-estimate the time the route might get lost.

Let's assume we found and measured some connections and ended up with something like this:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | **8.8**       | 999     | 999     | **9**       | **8**        |
| **CP2**     | **13**        | **15**      | 999     | **5**       | 999      |
| **CP3**     | **15**        | 999     | **12.5**    | 999     | **7**        |
| **CP4**     | 999       | **2**       | **12**      | **16.3**    | 999      |
| **Finish**  | 999       | **12**      | 999     | **17**      | **25**       |

The last step is to save this table in CSV format. That means we want to have a file where each row of the table is a new line and values between columns are separated by something like commas, spaces, tabs, etc. Here's how the table could look like:

```
8.8	999	999	9	8
13	15	999	5	999
15	999	12.5	999	7
999	2	12	16.3	999
999	12	999	17	25
```

Currently the implementation rounds up the times to the first decimal.

Note that the decimal part of a number uses decimal point and not a comma - that is important to keep in mind, because if you export your data from somewhere it might save those values with commas instead of decimal points and then the program won't be able to read the values correctly.

### Sequence dependence

Often how long it takes to go from CP `A` to CP `B` depends on from which way and at what speed you took CP `A` from. The program cannot understand entry angle or speed as it's highly track dependent, however this mostly depends on what CP was taken before CP `A`. 

That is imagine that going from CP `A` to CP `B` takes 10 seconds if we came from CP `D`, but same connection takes 13 seconds if we came from CP `E`, because in the first case we could carry the speed through CP `A` and in the second case we had to stop and turn around.

In order to input that data into spreadsheet we would write for connection from `A` to `B`: `10(D)13(E)` where D and E would be CP numbers. For start we use number 0. At the end of such chain it's also worth adding unconditional time it would take to go from `A` to `B` like using standing-respawn. So if we say in that case it takes 14 seconds it would look like this: `10(D)13(E)14`. Additionally we can add multiple CPs in the brackets. Say the connection from `A` to `B` also takes 10 seconds when coming from CP `G`. In that case we have: `10(D,G)13(E)14`. 

That this system is also useful for specifying if some connection is only possible from limited set of previous CPs. For example if connection from CP `A` to CP `B` takes 8 seconds, but can only be reasonably done with a lot of speed which can only be done if we came to CP `A` from CP `H` or `J` we would have in that case: `8(H,J)`.

You can see a full real example spreadsheet using this system [here](example%20input%20data/%5BRPG%5D%20Evergreen.csv)

In general I would recommend the workflow as described at that start of this page. That is we start by inputting the best possible times without specifying the sequence dependence and then going back and only adding the sequence dependent times to the connections that are actually used by the fastests found routes. Otherwise you are going to be wasting time measuring different sequence dependent times for connections that are never used in the good solutions and it's easy to make a mistake and miss adding some CP to the "fast" version of the connection.

So if say I calculated the best possible time to go from `CP 3` to `CP 5` is `8.8` sec and then in the solutions I see the best paths take `...,1,3,5,...` I go back and measure how long `3->5` connection takes given that I have `CP 1` before `CP 3` and say in that case it comes out to `10.6` sec. In that case I change the cell with `3->5` connection from `8.8` to `10.6(1)8.8`. To help with this process you can use [verified connections list](#verified-connections-list) as described below.

### Verified connections list

First I will describe the syntax and below explain what is the purpose of this feature.

Right under the input spreadsheet you can add a line starting with `#` symbol and then in the following lines input the list of sequence dependent connections. As an example using the example spreadsheet from before this is how it could look like:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|---------|
| **CP1**     | 8.8       | 999     | 999     | 9       | 8       |
| **CP2**     | 13        | 15      | 999     | 5       | 999     |
| **CP3**     | 15        | 999     | 12.5    | 999     | 7       |
| **CP4**     | 999       | 2       | 12      | 16.3    | 999     |
| **Finish**  | 999       | 12      | 999     | 17      | 25      |
|             | **#**     |**Prev** |**From** | **To**  |**Time** |
|             |           | 0       | 1       | 2       | 0       |
|             |           | X       | 0       | 1       | 0       |
|             |           | 2       | 4       | 1       | 2.3     |
|             |           | 1       | 2       | 4       | Set 15  |

The first value specifies the leading/previous CP number and the next 2 values specify the actual connection From/To CP numbers. The last value is a time change for a given connection. In case you want to specify "Any" previous/leading CP you need to input the `X` letter. It's also required in case the `From` CP is `Start` (CP 0) since there is no previous CP in that case - you can see this in the 2nd line of the example above. `Finish` in this case would be CP 5.

If you input a line with **Time** 0 it won't change any connection times, but it will consider that connection as "verified" which means that you checked that this dependent connection time is correctly reflected in the table above. 
If you input non-zero **Time** it will add that time to the connection in the table. That is for example the result of the 3rd line will be the same as if you had `10.3(2)8` instead of `8` in the CP 4 -> CP 1 cell and had the same verified connection line with time = 0 - `2 | 4 | 1 | 0`.
If you want to "Set" a new time instead of adding to the existing value in the table you have to write `Set` before the time. So in the case of 4th line of the example it will be the same as having `15(1)12` in the CP 2 -> CP 4 cell.

To understand the purpose of this I think it's best to imagine workflow scenario. First we inputted the fastest possible connection times to the spreadsheet and generated initial solutions. Now we want to look at the connections used in the fastests routes and measure the sequence dependent times. However typically the top routes will have many connections in common. It's hard to keep track and remember which connections you have already checked and adjusted and also it's time consuming to go through each of the fastests routes and find the possibly missed non-adjusted sequence dependent connections. This feature helps you keep track of that. In the program results you can see under **Unver.** column the number of "unverified" connections for each of the solutions found. You can also click that button to get a full list of the "unverified" connections that you can copy and paste to the spreadsheet and check.

As a bonus this will also highlight routes that are significantly different from the ones you have already checked. Once the top solution/s are fully verified if some other good solution has a high number of "unverified" connections it means it's significantly different from the other top solutions and is worth looking at (even if it's probably bad, since usually after verifying connections the route time will increase).

If it comes to what you input in **Time** there are 2 normal ways you can go about it:
- Add sequence dependent times directly into spreadsheet using brackets `()` syntax as desribed in [sequence dependence](#sequence-dependence) and always have **Time** = 0 (Use this feature only for connection verification)
- Leave the spreadsheet as is (having only the fastests possible times) and adjust the times using the delta/`Set` **Time** in the verified connections list.

You can mix both of those even for the same connection (delta will be added to the sequence dependent time if it's already in the main table and `Set` time will overwrite it no matter what), but it's probably just going to be confusing so I wouldn't recommend that.

### Google spreadsheet template

Here's [google spreadsheet template](https://docs.google.com/spreadsheets/d/1Cjsf0P_ye-ZRNcDmxpv4Nd77UHDDQyyKQYvH25s_mfw/edit?gid=331322614#gid=331322614) to help quickly create google spreadsheets that have format compatible with automatic downloading of spreadsheet by the program. You need to use and save that template in your own Google account for this to work. If you just want to see/copy an example 20 CP spreadsheet template you can get it on the [2nd tab of the same template spreadsheet](https://docs.google.com/spreadsheets/d/1Cjsf0P_ye-ZRNcDmxpv4Nd77UHDDQyyKQYvH25s_mfw/edit?gid=675531852#gid=675531852).

To use it just follow the instructions on the **Start** tab.

In case of automatic download feature in the program the way it works is after you download spreadsheet for the first time it gets saved in `AppData\Local\TrackmaniaPathFinder` directory under the name `{SpreadsheetID}#{SheetID}.csv`. After that you can either redownload new version on the next algorithm run using the checkbox or just run the last downloaded local version. The program will find the local version itself using the google spreadsheet URL, you don't need to find that file in `AppData` yourself.

The downloading of spreadsheet is very crude - it simply downloads everything in range `B2:ZZ300` (so ignoring first row and column) and saves it to a local file. This means your actual spreadsheet data has to start at `B2` cell, just as it is in the template spreadsheet, otherwise it's not going to work properly.

## Program usage

First you need to download the program [TmPathFinder.exe](https://github.com/isfoo/TrackmaniaPathFinder/releases/latest/download/TmPathFinder.exe "TmPathFinder.exe").
It's a GUI application where you configure parameters and run the search for fastests routes.

If you want to change the font size use ctrl + mouse scroll.
Font size and all input fields get saved between program runs in `AppData\Local\TrackmaniaPathFinder\inputData.txt` file. If you want to reset to defaults click the red button **Reset all to default values**.

Almost every input has a **(?)** tooltip you can mouse over to read about what it does.

### Basic path finding

On **Path Finder** tab you need to provide the input spreadsheet with the format as described in [Input spreadsheet](#input-spreadsheet) section. It can either be a local file you input to **input data file** field or URL to google spreadsheet sheet you input to **input data link**. You can only use one option at a time so if you want to swtich to the other option clear the text box of the other one.

Once you provide the input you can click **Run exact algorithm** and it's going to generate the solutions in table below. Once it's completed you will see **Status** change to **Done**. The solutions found are guaranteed to be optimal top **max nr of routes** solutions. Note that if status will change to **Timeout** it means the algorithm was not able to finish before **max search time** and in that case there are no guarantees on the quality of solutions found. By default **max search time** is set to `0` which means it's infinite.

In the output, format of which I will describe below, you might see numbers in parentheses. Those are "repeat CPs". Best way to understand repeat CPs is by example. Say in my spreadsheet I wrote that going from CP1 to CP2 takes 12 seconds and going from CP2 to CP3 takes 5 seconds. I didn't write anything for CP1 to CP3 connection, because I couldn't find anything good. However actually if you go through CP2 you can get from CP1 to CP3 in 12+5=17 seconds. It might actually be the case that going through CP2 multiple times is worth it and required for the optimal route. Allowing this option it would find routes with that repeated CP2. Initially you might think it's rare for such connections to be useful, but that is not the case. Often routes end up having key CPs like for example ones with easy access to reactor boost that let you go quickly to many points of the map that otherwise would take much longer. Also it means you don't have to manually input such connections in the input spreadsheet as this program will do it for you.

It's important to note that repeat CPs can work unexpectedly when combined with sequence dependence. if your route would have a path like this: `1,(3),5,7` then for the `5->7` connection the program for the purpose of sequence dependence will consider `CP 1` as the preceding CP and the the `CP 3`. To avoid confusion in cases that sequence dependence matters it's probably best to manually input the `1->5` connection to the spreadsheet, even if the path goes through `CP 3`.

If the program takes too long to complete see [Advanced path finding](#advanced-path-finding). Also in this case it probably means you have a big spreadsheet you worked a lot on so you should consider contacting me through discord [@isfoo](https://discordapp.com/users/552077071333982219 "@isfoo") and I will be happy to help ^^

### Advanced path finding

At the very top of the program enable the checkbox **show advanced settings**.

Most of the new options should be understable upon reading the **(?)** tooltip text. In this section I will explain the usage of **heuristic algorithm** and you can read about the [connection finder mode](#connection-finder-mode) below.

As long as **Run exact algorithm** completes in a reasonable ammount of time you should always prefer it to **Run heuristic algorithm**. Upon completion the **Exact** algorithm will always find the best routes, while **Heuristic** algorithm might miss some of them. However if your spreadsheet is very big or is medium sized with tons of sequence dependence connections/ring CPs the **Exact algorithm** might not be fast enough. If that is the case here's are the recommended steps you should try in order:

1. Decrease **max nr of routes**. This is the main parameter that increases the search time. If you set it too high initially you should try decreasing this value.

2. Decrease **max route time**. You should set it something closer to the expected time the fastests routes should take. Of course you might not know that value, but if you set it to something too low the worst thing that can happen is the program will end without finding any route which will tell you that there are no possible routes with that time or lower and you can try increasing this value.

3. Switch to **Run heuristic algorithm**. In that case feel free the set back **max nr of routes** and **max route time** to whatever you want - it won't make a difference for heuristic algorithm. You should **NOT** expect to finish running the algorithm until the status changes to **Done**. Most of the time the algorithm will find most or all top 100 solutions in the first 10 seconds, but then it will continue trying possibly for hours/days or longer depending on the problem, often not finding anything new (as there might be nothing new to find). In practice you should look at 2 things. 1st how does **Candidates found** changes while running the algorithm. If it stays at the same value or barely changes then likely there is not much more to find. 2nd thing to look at is **Search progress**. You will see there `Completed X tries for K-opt`. You should run the program such that at the end you see in that text at least `K > 7` or `K = 7` and `X >= 100`. Note that this algorithm although in practice seems to work very well, unlike the **Exact** algorithm even if it completes running (status **Done**) there are no guarantees on solution quality.

Additionally **ring CPs** can be very detrimental to the program runtime while not giving that much in return. If you set **ring CPs** you can try removing some/all of them. Note that in that case it will simply find all routes that don't use standing respawn. If you think using standing respawn is faster, often you will be able to tell when it's best to do it. In that case you can also consider creatinh second version of the input spreadsheet that doesn't contain ring CPs and use this program to find routes between normal CPs and just take ring CPs when it's fastests.

### Output format

The routes are saved in a pretty straightforward format that's best seen through examples:

```115.0 [Start,8,2-5,1,6-7,Finish]```

This means the route takes 115 seconds and the order is: Start -> CP8 -> CP2 -> CP3 -> CP4 -> CP5 -> CP1 -> CP6 -> CP7 -> Finish

In case of repeat CPs the repeated CPs are shown in brackets ```()```. For example:

```154.3 [Start,8,5,9,3,(5),4,(9,3),1-2,6-7,Finish]```

This means the route is: Start -> CP8 -> CP5 -> CP9 -> CP3 ->(go through CP5)-> CP4 ->(go through CP9 then CP3)-> CP1 -> CP2 -> CP6 -> CP7 -> Finish

In case of ring CPs the standing respawn is shown as letter `R`. For example:

```74.0 [Start,7,4,R,1-3,R,5-6,Finish]```

This means CP4 and CP3 are ring checkpoints and the route is: Start -> CP7 -> CP4 ->(standing-respawn at CP7)-> CP1 -> CP2 -> CP3 ->(standing-respawn at CP2)-> CP5 -> CP6 -> Finish

### Connection finder mode

When working on creating a spreadsheet it's easy to miss some important connections. In order to help find potentially good connections that are missing from your current spreadsheet you can use the **Connection finder mode**.

As the very last option in the **Path Finder** tab there is **Connection finder mode** checkbox. When it's enabled the program changes its behavior. The program will insert **tested time** time into every connection that matches the conditions set by the other options in the newly expanded settings under **Connection finder mode** checkbox. Then for each such newly created input with that single connection change it will run the program like normal and find top 1 solution.

This way you can check the existance of which new connections could have a huge beneficial impact on the results and are worth a closer look.

### Other tabs

**CP positions creator** tab allows you to create a text file where each line is a `x,y,z` position of every CP (+Start/Finish). It's done using a .gbx replay or ghost file and the position is taken based on your car position at the time you drove through the CP trigger. This means it's not perfect and different replays can give slightly different results.

This file can be used in a program in 2 ways:
- you can use it on **Path Finder** tab in **CP positions file** field. This will allow you to change **Graph** window to **real view** - that is you can see visually how each of the found routes looks like.
- you can use it on **Distance matrix creator** tab. This will allow you to create an input spreadsheet where instead of times you have distances between CPs. This can be very useful on very open maps without too many elevation changes since often the paths with the shortest distance will also be pretty quick. Ofc shortest path will likely not be optimal, but it's still a good way to very quickly get an idea what has a potential to be good.

**Replay visualizer tab** takes .gbx replay or ghost file and shows the 2D path the car traces.

## Examples of real data used for reroutes

| **Map** | **CP count** | **Spreadsheet** | **Spreadsheet creator** |
|-------------|-----------|---------|---------|
| [Macopolis RPG](https://trackmania.exchange/maps/112275/macopolis-rpg) | 25 | [spreadsheet](example%20input%20data/Macopolis%20RPG.csv) | isfoo |
| [[RPG] Evergreen](https://trackmania.exchange/maps/156959/rpg-evergreen) | 22 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Evergreen.csv) | isfoo |
| [MTC - Castle of Confusion](https://tm.mania-exchange.com/maps/121329/mtc-castle-of-confusion) | 18 | [spreadsheet](example%20input%20data/MTC%20-%20Castle%20of%20Confusion.csv) | Lars_tm
| [Sobekite Eternal 2020](https://tm.mania-exchange.com/maps/182399/sobekite-eternal-2020) | 25 | [spreadsheet](example%20input%20data/Sobekite%20Eternal%202020.csv) | Lars_tm
| [[RPG] Biozone](https://trackmania.exchange/maps/85912/rpg-biozone) | 25 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Biozone.csv) | Lars_tm
| [[RPG] Catsuya](https://trackmania.exchange/maps/95028/rpg-catsuya) | 109 | [spreadsheet](example%20input%20data/%5BRPG%5D%20Catsuya.csv) | Lars_tm |
| [World of Wampus 6](https://trackmania.exchange/maps/111213/world-of-wampus-6) | 100 | [spreadsheet](example%20input%20data/World%20of%20Wampus%206.csv) | Lars_tm |
| [World of Wampus 7](https://trackmania.exchange/maps/138791/world-of-wampus-7) | 100 | [spreadsheet](example%20input%20data/World%20of%20Wampus%207.csv) | Lars_tm |

Note that even the 100+ CP Lars spreadsheets are easily processed using exact algorithm.

## Implementation details

The problem solved by this program can be most accurately described as finding the top N lowest weight hamiltonian paths in directed graph with sequence dependent weights. Since problem of finding minimum weight hamiltonian path is a relaxed version of traveling salesman problem, generally in literature the algorithms are made and decsribed for the latter. Thus the algorithms used are made for finding top N solutions to SDATSP (Sequence Dependent Asymmetric Traveling Salesman Problem).

Although You will find online some efficient programs that solve TSP there are a couple of problems: 
1. It's hard to find a program that is fast and easy to use - most good solvers are using command line interface or are just an API in some programming language.
2. Many solvers don't natively support Asymmetric versions of TSP. For example [Concorde](https://www.math.uwaterloo.ca/tsp/concorde.html) which is consider one the best exact solvers doesn't support ATSP. Note that it's possible to convert ATSP to TSP problems by creating dummy nodes which is desribed well [here](https://www.linkedin.com/posts/andreas-beham-004279b_optimizing-asymmetric-travelling-salesperson-activity-7107809854215323648-FfuJ).
3. No support for control over repeat nodes (aka repeat CPs)
4. No ability to find top N solutions and not just a single one. This is a big one, because it's an important feature. There is a lot of hidden heuristic knowledge only the player generating solution has that can ultimatley decide which routes are truly the best, but to do that in practice one has to see a list of solutions. The factors are things like number of hard CPs in route and where the connections are (It's usually best to have hard connections first and easy connections last). I can also say from experience that once you have a list of routes you start to piece together a better picture of the map and it makes you realise which additional connections would be good and it leads you to finding new great connections.
5. No support for sequence dependence (that includes ring CPs)
6. Lack of additional useful supporting features like "connection finder" or "verified connections list".

Here's how the current implementations work:

### Exact algorithm

Of course it is best if we can fully solve the problem - that is finding actual top N solutions and not just N good solutions that may or may not be the actual top solutions. After all we don't want to miss some great routes.

In general first thing to note is that for exact algorithms you essentially have to check all possible paths. The differences between possible algorithms one could use are about how to try to skip checking as many paths as possible by proving they cannot possibly be fast enough. As an easy example say some connection between nodes `A` and `B` has assigned `cost=500`. If we already found a path with `cost=300` then we know for a fact that optimal solution cannot possibly contain `A->B` connection since the cost of that connection alone is higher than cost of current optimum known path so we can throw away all combinations that contain `A->B` connection.

This program uses **Branch and Bound** method with **Assignment relaxation** solved using **Hungarian method**.

**Branch and Bound** refers to the process of going through all possible routes where we try to check most promising routes first (that's the **Branch** part) and where we do this early stopping as desribed above (that's the **Bound** part).

**Assignment relaxation** is the bounding / early stopping method. **Assignment problem** is a simpler version of **ATSP** that can be solved quickly. Instead of finding the lowest cost `N+1` edges that form a single cycle we find lowest cost set of `N+1` edges such that each of the `N` nodes has exactly `1` out edge and exactly `1` in edge. It means that every single possible **ATSP** solution is also possible **Assignment problem** solution, however not all possible **Assignment problem** solutions are possible **ATSP** solutions since in the former we could have multiple cycles and not just one.

**Hungarian method** is an algorithm solving **Assignment problem**. A good simple explaination of this algorithm can be found [here](https://www.hungarianalgorithm.com/examplehungarianalgorithm.php)

So here's how the algorithm works. Note that it's only a rough description of general idea:

Input: `N` by `N` matrix with edge costs; `K` - number of best solutions to find.

1. Solve assignment problem using **Hungarian method** and save minimal cost of assignment as **LowerBound**.
2. If **LowerBound** is bigger than the cost of already found top `K` solutions then **return** (early stopping)
3. Otherwise choose the edge `E` that is least likely to be part of any solution. We do this by going through the edges used in optimal assignment and estimating how much **LowerBound** would increase if we were to remove this edge. The edge without which the **LowerBound** would increase the most is our chosen edge.
4. Fork into 2 possible recursive paths:
    1. Lock-in edge `E` - Remove all edges that cannot be part of the solution together with this edge. Go to step 1. 
    2. Remove edge `E` - Set the value of that edge to infinity. Go to step 1.

Since the algorithm tries for each edge to either include it at step 4.1 or exclude it at step 4.2 we are guaranteed to go through all possible combinations. However because at each step we calculate current **LowerBound** on minimal cost of the solution we will be able to relatively quickly realize there is no point in recursing futher and can skip tons of routes.

#### Sequence dependence

The program supports different weights depending on previous node in path. Note that this allows to model the speed/angle of entry by proxy, but also partially model ring CPs. Let's say I have a connection from CP 2 to CP 3 and from CP 2 to CP 4. If CP 3 is a ring CP this simply means that I can add a connection between CP 3 and CP 4 with the same cost as going from CP 2 to CP 4, as long as CP 2 was preceding CP 3, otherwise this connection doesn't exist.

The desribed above algorithm doesn't natively support such sequence dependent weights. What we do is we create a graph that for each conditional connection takes its lowest possible value. That is we assume each connection will happen to end up in a best case scenario. We use this new graph for the above algorithm. During the algorithm when some connection is removed we can update the graph weights if after removing a connection the minimum weight has increased.

### Heuristic algorithm

If exact method is too slow we can use a heuristic one. The difference is that this kind of algorithm doesn't give guarantees on solution quality, but should generally complete the search much faster and in practice will give good and even most likely optimal solutions. For this purpose I implemented algorithm based on [Lin–Kernighan heuristic](https://en.wikipedia.org/wiki/Lin%E2%80%93Kernighan_heuristic). 

I made some changes to the algorithm so it works well for the purpose of this program. The 3 main things typical algorithm and it's implementations don't care about are:
1. Directed graph - We want it to work well for ATSP and not (symmetric) TSP
2. Sequence dependence - We need the algorithm to take into consideration sequence dependent connections
3. Finding a lot of solutions - Lin–Kernighan heuristic was made with finding a single optimal solution in mind, but we want a lot of them.

I won't describe how Lin–Kernighan heuristic works, but will give a very non-complete simplified description of the ideas used. If you want to learn more about Lin–Kernighan heuristic I recommend first part of this [paper](https://sci-hub.se/10.1016/S0377-2217(99)00284-2) by Keld Helsgaun and this [series of blog posts](https://tsp-basics.blogspot.com/).

The base idea is simple. First we generate some initial complete route. The initial route can be completely random - it doesn't need to be good. Then we perform a series of small changes to the route that make it better until we can no longer make any more changes. This results in a locally optimal solution. To increase the chances that this solution is globally optimal we can do 2 things - increase the size of changes we make to the route and redoing the process multiple times with different starting routes to hit different local optimums hoping that one of them will be the global optimum.

The *change* of route for our purposes is a **k-opt move**. **k-opt move** means we remove **k** connections from our complete route and add **k** connections such that we get a new complete route that has lower total cost. A route is **k-opt** if no possible **k-opt** move for it exists. Few things to note:
- If route is **N-opt** where N is the number of nodes (CPs) then it means it's globally optimal.
- All routes are **1-opt** - if you remove a single connection the only way to get a valid route is to add that same connection back.
- For ATSP all routes are **2-opt** - harder to explain, but if you try it you will realize if you remove 2 connections the only way to get a valid route is to add those 2 connections back.

The algorithm tries making those **k-opt** moves for varying values of **k** until it's no longer able to do so and at that point it starts the process again with a new starting route.
