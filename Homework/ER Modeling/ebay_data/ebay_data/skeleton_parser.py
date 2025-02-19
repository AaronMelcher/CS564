
"""
FILE: skeleton_parser.py
------------------
Author: Firas Abuzaid (fabuzaid@stanford.edu)
Author: Perth Charernwattanagul (puch@stanford.edu)
Modified: 04/21/2014

Skeleton parser for CS564 programming project 1. Has useful imports and
functions for parsing, including:

1) Directory handling -- the parser takes a list of eBay json files
and opens each file inside of a loop. You just need to fill in the rest.
2) Dollar value conversions -- the json files store dollar value amounts in
a string like $3,453.23 -- we provide a function to convert it to a string
like XXXXX.xx.
3) Date/time conversions -- the json files store dates/ times in the form
Mon-DD-YY HH:MM:SS -- we wrote a function (transformDttm) that converts to the
for YYYY-MM-DD HH:MM:SS, which will sort chronologically in SQL.

Your job is to implement the parseJson function, which is invoked on each file by
the main function. We create the initial Python dictionary object of items for
you; the rest is up to you!
Happy parsing!
"""

import sys
from json import loads
from re import sub

columnSeparator = "|"

# Dictionary of months used for date transformation
MONTHS = {'Jan':'01','Feb':'02','Mar':'03','Apr':'04','May':'05','Jun':'06',\
        'Jul':'07','Aug':'08','Sep':'09','Oct':'10','Nov':'11','Dec':'12'}

"""
Returns true if a file ends in .json
"""
def isJson(f):
    return len(f) > 5 and f[-5:] == '.json'

"""
Converts month to a number, e.g. 'Dec' to '12'
"""
def transformMonth(mon):
    if mon in MONTHS:
        return MONTHS[mon]
    else:
        return mon

"""
Transforms a timestamp from Mon-DD-YY HH:MM:SS to YYYY-MM-DD HH:MM:SS
"""
def transformDttm(dttm):
    dttm = dttm.strip().split(' ')
    dt = dttm[0].split('-')
    date = '20' + dt[2] + '-'
    date += transformMonth(dt[0]) + '-' + dt[1]
    return date + ' ' + dttm[1]

"""
Transform a dollar value amount from a string like $3,453.23 to XXXXX.xx
"""

def transformDollar(money):
    if money == None or len(money) == 0:
        return money
    return sub(r'[^\d.]', '', money)

"""
Formats the strings to match specifics needed for loading
- Removes leading/trailing spaces
- Escapse quotes
- Adds quotes to end/beginning of strings
"""
def format_string(input: str) -> str:
    if input is None:
        return 'NULL'

    formatted = input.strip()
    formatted = formatted.replace('"', '""')
    formatted = f'"{formatted}"'
    return formatted

"""
Parses a single json file. Currently, there's a loop that iterates over each
item in the data set. Your job is to extend this functionality to create all
of the necessary SQL tables for your database.
"""
def parseJson(json_file):
    # track currently added users to deal with duplicates
    users = set()
    with open(json_file, 'r') as f:
        items = loads(f.read())['Items'] # creates a Python dictionary of Items for the supplied json file
        for item in items:
            """
            TODO: traverse the items dictionary to extract information from the
            given `json_file' and generate the necessary .dat files to generate
            the SQL tables based on your relation design
            """
            # load each part of the Item information
            item_data = []
            item_data.append(item["ItemID"])
            item_data.append(format_string(item["Name"]))
            item_data.append(format_string(transformDollar(item["Currently"])))

            # Check for existence of Buy Price in Item
            if "Buy_Price" in item:
                item_data.append(format_string(transformDollar(item["Buy_Price"])))
            else:
                item_data.append(format_string(None))

            item_data.append(format_string(transformDollar(item["First_Bid"])))
            item_data.append(item["Number_of_Bids"])
            item_data.append(format_string(transformDttm(item["Started"])))
            item_data.append(format_string(transformDttm(item["Ends"])))
            item_data.append(format_string(item["Description"]))

            item_result = '|'.join(item_data)

            # load new Category data (if applicable)

            # iterate through the bids related to the item
            if item["Bids"] is not None:
                for bid in item["Bids"]:
                    bid_data = []
                    bid_data.append(item["ItemID"])
                    
                    bidder_id = format_string(bid["Bidder"]["UserID"])
                    bid_data.append(bidder_id)

                    bid_data.append(format_string(transformDttm(bid["Time"])))
                    bid_data.append(format_string(transformDollar(bid["Amount"])))

                    bid_result = "|".join(bid_data)

                    # load bidder data
                    # make sure user hasnt been loaded previously
                    if bidder_id not in users:
                        bidder_data = []
                        bidder_data.append(bidder_id)
                        bidder_data.append(bid["Bidder"]["Rating"])
                        bidder_data.append(bid["Bidder"]["Location"])
                        bidder_data.append(bid["Bidder"]["Country"])

                        bidder_result = "|".join(bidder_data)

            # load each part of the Seller information
            # make sure this user hasnt been loaded previously
            seller_id = format_string(item["Seller"]["UserID"])
            if seller_id not in users:
                seller_data = []
                seller_data.append(seller_id)
                seller_data.append(format_string(item["Seller"]["Rating"]))
                seller_data.append(format_string(item["Location"]))
                seller_data.append(format_string(item["Country"]))

                seller_result = "|".join(seller_data)

            pass

"""
Loops through each json files provided on the command line and passes each file
to the parser
"""
def main(argv):
    argv = ["gang", "C:\\Users\\amelc\\Documents\\Repos\\CS564\\Homework\\ER Modeling\\ebay_data\\ebay_data\\items-0.json"]
    if len(argv) < 2:
        print >> sys.stderr, 'Usage: python skeleton_json_parser.py <path to json files>'
        sys.exit(1)
    # loops over all .json files in the argument
    for f in argv[1:]:
        if isJson(f):
            parseJson(f)
            print ("Success parsing " + f)

if __name__ == '__main__':
    main(sys.argv)
