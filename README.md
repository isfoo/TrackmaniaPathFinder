# TrackmaniaPathFinder

Program made to help find reroutes in Trackmania maps. The typical workflow for finding a reroute is:

1. Find all of the possible ways you can get from each CP to every other CP and write down in a spreadsheet the estimated time for each such connection.
2. Use this program to find all of the fastests possible routes that use those connections
3. Among found routes choose the best one.

The following sections explain how to use this program. I decided to write this more as a practical guide where I give some ideas on the general workflow of finding a reroute, rather than more technical specification. I do include some information on implementation details at the end for those who are interested.

If you have any questions / feature requests the best way is to contact me through discord [@isfoo](https://discordapp.com/users/552077071333982219 "@isfoo")

## Input spreadsheet

First you need to create spreadsheet containing N by N cells, where N is the number of CPs + 1 for the finish. So if I have map that has Start, CP1, CP2, CP3, CP4, Finish I have 5x5 table:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | 999       | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | 999     | 999     | 999     | 999      |
| **CP3**     | 999       | **999**     | 999     | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | 999     | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | 999      |

Where the row specifies from where Im going and the column specifies the destination. So for example in the above table the bolded cell will contain information about how long does it take to go from CP1 to CP3.

Initially the table should be filled with some big number, like that 999 in this example. This will mean that those connections takes 999 seconds. This arbitrary big value will be used to specify that given connection is not possible / should not be considered.

We can start filling in the table with the data we collected. Notice that the diagonal represents the normal route - going from Start to CP1, from CP1 to CP2, etc. Lets fill those in:

| **To\From** | **Start** | **CP1** | **CP2** | **CP3** | **CP4** |
|-------------|-----------|---------|---------|---------|----------|
| **CP1**     | **8.8**      | 999     | 999     | 999     | 999      |
| **CP2**     | 999       | **15**     | 999     | 999     | 999      |
| **CP3**     | 999       | 999     | **12.5**    | 999     | 999      |
| **CP4**     | 999       | 999     | 999     | **16.3**    | 999      |
| **Finish**  | 999       | 999     | 999     | 999     | **25**       |

Now we can fill the rest. Of course you only need to fill as much as you want or can - after all some connections might not even be possible. 

Note that in practice you don't have to exactly measure the time before you input it to the table - you can initially fill in good guesses about how long some connections take and only after you find the best possible routes you can go back and measure the connections used in those routes more accurately. This will save you time, because likely a lot of the connections will not even end up in any possible route, so measuring the time accurately will be a waste of time.

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

Note that the decimal part of a number uses decimal point and not a comma - that is important to keep in mind, because if you export your data from somewhere it might save those values with commas instead of decimal points and then the program won't be able to read the values correctly.

## Basic program usage

First you need to download the program [TmPathFinder.exe](https://github.com/isfoo/TrackmaniaPathFinder/releases/latest/download/TmPathFinder.exe "TmPathFinder.exe").
It's a GUI application where you configure parameters and run the search for fastests routes.

Here's a basic explaination of the options:

**max route time** - maximum time a complete route from start to finish can take.

**max number of routes** - number of fastest routes you want to find. Unless you are working with a small number of connections you should not set it to an arbitrarily high value - this parameter plays a key role in how long the search process will take so you should set it to something reasonable.

**heuristic search depth** - see [Advanced program usage](#advanced-program-usage)

**output append data file** - path to the output file that will contain information about all the runs you performed and all the candidate routes found in the order they were found. Typically you won't be interested in this file, it's mostly there so that you can easily come back to earlier results for example if you accidently overwrite the **output data file**.

**output data file** - path to the output file which after completing running the algorithm will contain sorted list of top **max number of routes** found.

**input data file** - input file in CSV format with inserted connections as described in [Input spreadsheet](#input-spreadsheet) section. You can use **Find file** button to open windows explorer to pick the file or manually insert relative or absolute path to the file in the box to the right.

**allow repeat CPs** - best way to understand repeat CPs is by example. Say in my spreadsheet I wrote that going from CP1 to CP2 takes 12 seconds and going from CP2 to CP3 takes 5 seconds. I didn't write anything for CP1 to CP3 connection, because I couldn't find anything good. However actually if you go through CP2 you can get from CP1 to CP3 in 12+5=17 seconds. It might actually be the case that going through CP2 multiple times is worth it and required for the optimal route. Allowing this option it would find routes with that repeated CP2. Initially you might think it's rare for such connections to be useful, but that is not the case. Usually routes end up having key CPs like for example ones with easy access to reactor boost that let you go quickly to many points of the map that otherwise would take much longer. Also it means you don't have to manually input such connections in the input spreadsheet as this program will do it for you.

if you allow for repeat CPs you get additional options:

**max connections to add** - how many repeat connections should be added to input spreadsheet. You can set it to arbitrarily high number to find all such connections, however if you allow for too many it might significantly affect the search time.

**turned off repeat CPs** - As nice as repeat CPs can be, sometime they might lead to routes that are too hard. Imagine I have CP that allows for great connections, but the set up for them is pretty difficult (say some hard reactor flight). Allowing the reoute to go through that CP more than once might be too hard, because with repeat connections you don't have the ability to respawn and try again quickly, but have to first get to that CP. In that case this allows you to set a list of CPs you don't want to be considered in repeat connections. The list is separated by commas/spaces.

**Count repeat connections** - Clicking this button will calculate the max number of repeat connections that could be added.

**Run exact algorithm** - After you set all parameters this is the button you want to press to find the fastests routes. Once it's completed you will see **Status** change to **Done** which means you can find sorted list of the solutions in the **output data file**. You can also see the solutions found in the GUI application. The solutions found are guaranteed to be optimal.

**Run heuristic algorithm** - see [Advanced program usage](#advanced-program-usage)

## Advanced program usage

If you followed the instructions and clicked **Run exact algorithm** and the program takes too long to complete here's the list of things you should try in listed order:

1. Decrease **max number of routes**. This is the main parameter that increases the search time. If you set it too high initially you should try decreasing this value.

2. Decrease **max route time**. You should set it something closer to the expected time the fastests routes should take. Of course you might not know that value, but if you set it to something too low the worst thing that can happen is the program will end without finding any route which will tell you that there are no possible routes with that time or lower and you can try increasing this value.

3. If you set **allowed repeat CPs** and set **max connections to add** to high value (say above 100) then try decreasing that. If you care about those repeat CPs go to step 4.

4. If none of the above works you should switch to **run heuristic algorithm**. You can adjust **heuristic search depth** to increase how deep the search will go. With this algorithm almost for sure the very first solution found will be optimal, however in general there are no guarantees on solution quality. That is, unlike with **exact** algorithm, for example the 10th best solution the **heuristic** algortithm finds might actually only be 1000th best solution. Most of the time in practice it should work very well, however because it could miss some paths this algorithm should be last resort only when **exact** algorithm is too slow.

## Output format

The routes are saved in a pretty straightforward format that's best seen through examples:

```115.0 [Start,8,2-5,1,6-7,Finish]```

This means the route takes 115 seconds and the order is: Start -> CP8 -> CP2 -> CP3 -> CP4 -> CP5 -> CP1 -> CP6 -> CP7 -> Finish

In case of repeat CPs the repeated CPs are shown in brackets ```()```. For example:

```154.3 [Start,8,5,9,3,(5),4,(9,3),1-2,6-7,Finish]```

This means the route is: Start -> CP8 -> CP5 -> CP9 -> CP3 ->(go through CP5)-> CP4 ->(go through CP9 then CP3)-> CP1 -> CP2 -> CP6 -> CP7 -> Finish

## Implementation details

TODO
